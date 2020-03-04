#include "track_oracle.h"
#include "track_graph.h"

#include <cstring>  // memset

#include "common/vt_escapes.h"
#include "ui.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

static constexpr size_t MAX_TRAINS = 6;
static constexpr size_t BRANCHES_LEN = sizeof(Marklin::VALID_SWITCHES);

// EWMA with alpha = 1/4
inline static int ewma4(int curr, int obs) {
    return (3 * curr + obs) >> 2 /* division by 4 */;
}

class TrackOracleImpl {
   private:
    const TrackGraph track;
    const int uart;
    const int clock;
    const Marklin::Controller marklin;
    Marklin::BranchState branches[BRANCHES_LEN];

    // TODO: enumerate state (i.e: train descriptors, which track we're on, etc)
    train_descriptor_t trains[MAX_TRAINS];

    train_descriptor_t* descriptor_for(uint8_t train) {
        for (train_descriptor_t& t : trains) {
            if (t.id == train) return &t;
        }
        return nullptr;
    }

    int stopping_distance_mm(uint8_t train, uint8_t speed) const {
        // TODO use calibration data
        (void)train;
        (void)speed;

        return 250;  // 25cm
    }

    train_descriptor_t* attribute_sensor(Marklin::sensor_t sensor) {
        // TODO base this off of how far the sensor is from our prediction of
        // where each train is
        (void)sensor;

        for (train_descriptor_t& t : trains) {
            if (t.id != 0) return &t;
        }
        return nullptr;
    }

    int distance_between(const Marklin::track_pos_t& old_pos,
                         const Marklin::track_pos_t& new_pos) const {
        return track.distance_between(old_pos.sensor, new_pos.sensor,
                                      this->branches, BRANCHES_LEN) +
               (new_pos.offset_mm - old_pos.offset_mm);
    }

   public:
    // disallow copying, moving, or default-constructing
    TrackOracleImpl() = delete;
    TrackOracleImpl(const TrackOracleImpl&) = delete;
    TrackOracleImpl(TrackOracleImpl&&) = delete;
    TrackOracleImpl& operator=(const TrackOracleImpl&) = delete;
    TrackOracleImpl& operator=(TrackOracleImpl&&) = delete;

    TrackOracleImpl(int uart_tid, int clock_tid, Marklin::Track track_id)
        : track(track_id), uart{uart_tid}, clock{clock_tid}, marklin(uart_tid) {
        memset(trains, 0, sizeof(train_descriptor_t) * MAX_TRAINS);

        // TODO: actually have different inits for different tracks
        log_line(uart, "Initializing Track A...");

        // ensure the track is on
        marklin.send_go();

        log_line(uart, "Stopping all trains...");
        auto train = Marklin::TrainState(0);
        train.set_speed(0);
        train.set_light(false);
        for (uint8_t id : Marklin::VALID_TRAINS) {
            train.set_id(id);
            marklin.update_train(train);
        }

        marklin.flush();

        // set all the branches to curved
        for (size_t i = 0; auto& b : this->branches) {
            const uint8_t id = Marklin::VALID_SWITCHES[i++];

            b.set_id(id);
            b.set_dir(Marklin::BranchDir::Curved);

            // ...but make outer-ring branches straight
            for (size_t except_id : {6, 7, 8, 9, 14, 15}) {
                if (id == except_id) {
                    b.set_dir(Marklin::BranchDir::Straight);
                    break;
                }
            }
        }

        log_line(uart, "Setting switch positions...");
        marklin.update_branches(this->branches,
                                sizeof(Marklin::VALID_SWITCHES));
        marklin.flush();

        log_line(uart, "Track has been initialized!");
    }

    void calibrate_train(uint8_t id) {
        train_descriptor_t* train = nullptr;
        for (train_descriptor_t& t : trains) {
            if (t.id == 0) {
                train = &t;
                break;
            }
        }
        if (train == nullptr)
            panic("Cannot track more than %u trains!", MAX_TRAINS);

        log_line(uart, "Stopping train %hhu...", id);
        set_train_speed(id, 0);
        Clock::Delay(clock, 200);  // make sure it's slowed down

        log_line(uart, "Waiting for train to hit a sensor...");

        // give it some gas
        set_train_speed(id, 8);

        // loop until a sensor is hit
        Marklin::SensorData sensor_data;
        marklin.query_sensors(sensor_data.raw);  // clear residual sensor data
        Marklin::sensor_t sensor;

        while (true) {
            marklin.query_sensors(sensor_data.raw);

            auto sensor_opt = sensor_data.next_sensor();
            if (sensor_opt.has_value()) {
                sensor = sensor_opt.value();

                log_line(uart, "Train hit sensor %c%hhu!", sensor.group,
                         sensor.idx);
                break;
            }
        }

        set_train_speed(id, 0);  // stop the train
        int now = Clock::Time(clock);

        *train = {
            .id = id,
            .speed = 0,
            .reversed = false,
            .lights = false,
            .velocity = 0,
            .pos = {.sensor = sensor,
                    .offset_mm = stopping_distance_mm(train->id, 8)},
            .pos_observed_at = now,
            .speed_changed_at = now,
            // we can't predict when we will hit the next sensor since our speed
            // is 0
            .has_next_sensor = false,
            .next_sensor = sensor,
            .next_sensor_time = -1,

            .has_error = false,
            .time_error = 0,
            .distance_error = 0,
        };

        log_line(uart, "Done calibrating train %hhu...", id);
        Ui::render_train_descriptor(uart, *train);
    }

    void set_train_speed(uint8_t id, uint8_t speed) {
        auto train = Marklin::TrainState(id);
        train.set_speed(speed);
        train.set_light(true);
        marklin.update_train(train);

        train_descriptor_t* td = descriptor_for(id);
        if (td == nullptr) return;
        uint8_t old_speed = td->speed;
        td->speed = speed;
        td->lights = true;
        // TODO form a guess for the trains velocity until it hits the next
        // sensor
        int now = Clock::Time(clock);
        if (old_speed == 0 && speed > 0) {
            td->pos_observed_at = now;
        }
        if (old_speed != speed) {
            td->speed_changed_at = now;
        }
        Ui::render_train_descriptor(uart, *td);
    }

    void update_sensors() {
        Marklin::SensorData sensor_data;
        marklin.query_sensors(sensor_data.raw);

        int now = Clock::Time(clock);
        while (true) {
            auto sensor_opt = sensor_data.next_sensor();
            if (!sensor_opt.has_value()) break;
            Marklin::sensor_t sensor = sensor_opt.value();
            train_descriptor_t* td = attribute_sensor(sensor);
            if (td == nullptr) {
                log_line(uart,
                         "could not attribute sensor %c%hhu to any train",
                         sensor.group, sensor.idx);
                continue;
            }
            if (Marklin::sensor_eq(sensor, td->pos.sensor)) {
                // The sensor was triggered more than once - simply update
                // pos_observed_at.
                td->pos_observed_at = now;
                continue;
            }

            const Marklin::track_pos_t old_pos = td->pos;
            const Marklin::track_pos_t new_pos = {
                .sensor = sensor,
                .offset_mm =
                    0 /* TODO account for velocity and expected sensor delay */
            };
            int distance_mm = distance_between(old_pos, new_pos);
            int dt_ticks = now - td->pos_observed_at;
            if (dt_ticks <= 0) continue;
            int new_velocity_mmps = (TICKS_PER_SEC * distance_mm) / dt_ticks;

            if (now - td->speed_changed_at < 4 * TICKS_PER_SEC) {
                // For the first 4 seconds after a speed change the train could
                // be accelerating. As such, we expect to observe rapidly
                // changing velocities, so we set our velocity to the ovserved
                // velocity directly.
                td->velocity = new_velocity_mmps;
            } else {
                // Once a train has been cruising at the same speed level for a
                // time, we assume the train's velocity is close to constant. So
                // we use an exponentially weighted moving average to update our
                // velocity, smoothing out inconsitencies.
                td->velocity = ewma4(td->velocity, new_velocity_mmps);
            }
            td->pos = new_pos;
            td->pos_observed_at = now;

            if (td->has_next_sensor &&
                Marklin::sensor_eq(sensor, td->next_sensor)) {
                td->has_error = true;
                td->time_error = now - td->next_sensor_time;
                td->distance_error =
                    td->velocity * td->time_error / TICKS_PER_SEC;
            } else {
                // TODO if we miss a sensor, we could probably still extrapolate
                // this error
                td->has_error = false;
            }

            auto next_sensor_opt =
                track.next_sensor(sensor, branches, BRANCHES_LEN);
            if (next_sensor_opt.has_value()) {
                auto[sensor, distance] = next_sensor_opt.value();
                td->has_next_sensor = true;
                td->next_sensor = sensor;
                td->next_sensor_time =
                    now + ((TICKS_PER_SEC * distance) / td->velocity);
            } else {
                td->has_next_sensor = false;
            }

            Ui::render_train_descriptor(uart, *td);
            log_line(uart,
                     "observed train %d from %c%02hhu->%c%02hhu dx=%dmm "
                     "dt=%d.%02ds dx/dt=%dmm/s",
                     td->id, old_pos.sensor.group, old_pos.sensor.idx,
                     new_pos.sensor.group, new_pos.sensor.idx, distance_mm,
                     dt_ticks / TICKS_PER_SEC, dt_ticks % TICKS_PER_SEC,
                     new_velocity_mmps);
        }
    }
};

// ------------------------ TrackOracleTask Plumbing ------------------------ //

enum class MsgTag {
    CalibrateTrain,
    UpdateSensors,
    InitTrack,
    QueryBranch,
    QueryTrain,
    ReverseTrain,
    SetBranch,
    SetTrainLight,
    SetTrainSpeed,
    WakeAtPos,
};

struct Req {
    MsgTag tag;
    union {
        // clang-format off
        struct { Marklin::Track track; }                 init;
        struct { uint8_t id; }                           calibrate_train;
        struct { uint8_t id; Marklin::track_pos_t pos; } wake_at_pos;
        struct { uint8_t id; uint8_t speed; }            set_train_speed;
        struct { uint8_t id; bool active; }              set_train_light;
        struct { uint8_t id; }                           reverse_train;
        struct { uint8_t id; Marklin::BranchDir dir; }   set_branch_dir;
        struct { uint8_t id; }                           query_train;
        struct { uint8_t id; }                           query_branch;
        struct {}                                        update_sensors;
        // clang-format on
    };
};

struct Res {
    MsgTag tag;
    union {
        // clang-format off
        struct {} init;
        struct {} calibrate_train;
        struct {} wake_at_pos;
        struct {} set_train_speed;
        struct {} set_train_light;
        struct {} reverse_train;
        struct {} set_branch_dir;
        struct {} update_sensors;

        struct { train_descriptor_t desc; } query_train;
        struct { Marklin::BranchDir dir; } query_branch;
        // clang-format on
    };
};

void TrackOracleTask() {
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);

    assert(clock >= 0);
    assert(uart >= 0);

    log_line(uart, "Spawned TrackOracleTask!");

    int tid;
    Req req;
    Res res;
    memset((char*)&res, 0, sizeof(res));

    // First message needs to be an init command
    {
        int reqlen = Receive(&tid, (char*)&req, sizeof(req));
        if (reqlen <= (int)sizeof(req.tag))
            panic("TrackOracle: bad request length %d", reqlen);
        if (req.tag != MsgTag::InitTrack)
            panic("TrackOracle: expected InitTrack message");
    }

    // init the track oracle
    TrackOracleImpl oracle = TrackOracleImpl(uart, clock, req.init.track);

    // respond once the oracle has been instantiated
    res.tag = req.tag;
    Reply(tid, (char*)&res, sizeof(res));

    while (true) {
        int reqlen = Receive(&tid, (char*)&req, sizeof(req));
        if (reqlen <= (int)sizeof(req.tag))
            panic("TrackOracle: bad request length %d", reqlen);

        res.tag = req.tag;

        switch (req.tag) {
            case MsgTag::CalibrateTrain: {
                oracle.calibrate_train(req.calibrate_train.id);
            } break;
            case MsgTag::WakeAtPos: {
                panic("TrackOracle: WakeAtPos message unimplemented");
            } break;
            case MsgTag::SetTrainSpeed: {
                oracle.set_train_speed(req.set_train_speed.id,
                                       req.set_train_speed.speed);
            } break;
            case MsgTag::SetTrainLight: {
                panic("TrackOracle: SetTrainLight message unimplemented");
            } break;
            case MsgTag::ReverseTrain: {
                panic("TrackOracle: ReverseTrain message unimplemented");
            } break;
            case MsgTag::SetBranch: {
                panic("TrackOracle: SetBranch message unimplemented");
            } break;
            case MsgTag::QueryTrain: {
                panic("TrackOracle: QueryTrain message unimplemented");
            } break;
            case MsgTag::QueryBranch: {
                panic("TrackOracle: QueryBranch message unimplemented");
            } break;
            case MsgTag::UpdateSensors: {
                oracle.update_sensors();
            } break;
            default:
                panic("TrackOracle: unexpected request tag: %d", (int)req.tag);
        }

        Reply(tid, (char*)&res, sizeof(res));
    }
}

// ------------------------- TrackOracle Interface -------------------------- //

static inline void send_with_assert_empty_response(int tid, Req req) {
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return;
}

TrackOracle::TrackOracle(Marklin::Track track) {
    this->tid = Create(1000, TrackOracleTask);
    Req req = {.tag = MsgTag::InitTrack, .init = {.track = track}};
    send_with_assert_empty_response(this->tid, req);
}

/// Called whenever a new train is placed on the track. Sets the train speed
/// to zero, waits a bit, sets the speed to something fairly low, and waits
/// for it to hit a sensor (thereby determining it's position and direction)
void TrackOracle::calibrate_train(uint8_t train_id) {
    Req req = {.tag = MsgTag::CalibrateTrain,
               .calibrate_train = {.id = train_id}};
    send_with_assert_empty_response(this->tid, req);
}

/// Update a train's speed
void TrackOracle::set_train_speed(uint8_t id, uint8_t speed) {
    Req req = {.tag = MsgTag::SetTrainSpeed,
               .set_train_speed = {.id = id, .speed = speed}};
    send_with_assert_empty_response(this->tid, req);
}
/// Update a train's lights
void TrackOracle::set_train_light(uint8_t id, bool active) {
    Req req = {.tag = MsgTag::SetTrainSpeed,
               .set_train_light = {.id = id, .active = active}};
    send_with_assert_empty_response(this->tid, req);
}
/// Reverse a train's direction (via speed 15)
void TrackOracle::reverse_train(uint8_t id) {
    Req req = {.tag = MsgTag::ReverseTrain, .reverse_train = {.id = id}};
    send_with_assert_empty_response(this->tid, req);
}

/// Update a branch's direction
void TrackOracle::set_branch_dir(uint8_t id, Marklin::BranchDir dir) {
    Req req = {.tag = MsgTag::SetBranch,
               .set_branch_dir = {.id = id, .dir = dir}};
    send_with_assert_empty_response(this->tid, req);
}

void TrackOracle::update_sensors() {
    Req req = {.tag = MsgTag::UpdateSensors, .update_sensors = {}};
    send_with_assert_empty_response(this->tid, req);
}

/// Return a particular train's current state
train_descriptor_t TrackOracle::query_train(uint8_t id) {
    Req req = {.tag = MsgTag::QueryTrain, .query_train = {.id = id}};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return res.query_train.desc;
}

/// Return a particular branch's state
Marklin::BranchDir TrackOracle::query_branch(uint8_t id) {
    Req req = {.tag = MsgTag::QueryBranch, .query_branch = {.id = id}};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return res.query_branch.dir;
}

/// Unblock the calling task once the specified train is at the specified
/// position on the track
void TrackOracle::wake_at_pos(uint8_t train_id, Marklin::track_pos_t pos) {
    Req req = {.tag = MsgTag::WakeAtPos,
               .wake_at_pos = {.id = train_id, .pos = pos}};
    send_with_assert_empty_response(this->tid, req);
}
