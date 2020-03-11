#include "track_oracle.h"
#include "track_graph.h"

#include <climits>
#include <cstring>
#include <optional>

#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "calibration.h"
#include "ui.h"

// ------------------------ TrackOracleTask Plumbing ------------------------ //

enum class MsgTag {
    CalibrateTrain,
    InitTrack,
    MakeLoop,
    Normalize,
    QueryBranch,
    QueryTrain,
    ReverseTrain,
    SetBranchDir,
    SetTrainLight,
    SetTrainSpeed,
    Tick,
    UpdateSensors,
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
        struct {}                                        tick;
        struct {}                                        make_loop;
        Marklin::track_pos_t                             normalize;
        // clang-format on
    };
};

struct Res {
    MsgTag tag;
    union {
        // clang-format off
        struct {} init;
        struct {} calibrate_train;
        struct { bool success; } wake_at_pos;
        struct { bool success; } set_train_speed;
        struct { bool success; } set_train_light;
        struct { bool success; } reverse_train;
        struct {} set_branch_dir;
        struct {} update_sensors;
        struct { bool valid; train_descriptor_t desc; } query_train;
        struct { Marklin::BranchDir dir; } query_branch;
        Marklin::track_pos_t normalize;
        struct {} make_loop;
        // clang-format on
    };
};

static constexpr size_t MAX_TRAINS = 6;
static const char* TRACK_ORACLE_TASK_ID = "TRACK_ORACLE";

// EWMA with alpha = 1/4
inline static int ewma4(int curr, int obs) { return (3 * curr + obs) / 4; }

/// Associates a tid with a position on the track that it should be woken up at
struct wakeup_t {
    int tid;
    uint8_t train;
    Marklin::track_pos_t pos;
};

class TrackOracleImpl {
   private:
    TrackGraph track;
    const int uart;
    const int clock;
    const Marklin::Controller marklin;

    train_descriptor_t trains[MAX_TRAINS];

    // TODO: support more than one blocked task
    std::optional<wakeup_t> blocked_task;

    int last_ticked_at;
    int max_tick_delay;

    // -------------------------- private methods -----------------------------

    train_descriptor_t* descriptor_for(uint8_t train) {
        for (train_descriptor_t& t : trains) {
            if (t.id == train) return &t;
        }
        return nullptr;
    }

    train_descriptor_t* attribute_sensor(Marklin::sensor_t sensor) {
        // TODO base this off of how far the sensor is from our prediction of
        // where each train is
        (void)sensor;

        for (train_descriptor_t& t : trains) {
            if (t.id == 0) continue;

            // check to see if the train's last sensor was this sensor
            if (Marklin::sensor_eq(t.pos.sensor, sensor)) {
                return &t;
            }

            // check to see if the next sensor is this sensor
            if (Marklin::sensor_eq(t.next_sensor, sensor)) {
                return &t;
            }

            // wait, maybe we missed a sensor!
            auto next_sensor_opt = track.next_sensor(t.next_sensor);

            if (next_sensor_opt.has_value()) {
                auto [next_sensor, _] = next_sensor_opt.value();
                if (Marklin::sensor_eq(sensor, next_sensor)) {
                    return &t;
                }
            }

            // otherwise, it's not this one...
        }
        return nullptr;
    }

    std::optional<int> distance_between(
        const Marklin::track_pos_t& old_pos,
        const Marklin::track_pos_t& new_pos) const {
        auto distance_opt =
            track.distance_between(old_pos.sensor, new_pos.sensor);
        if (!distance_opt.has_value()) return std::nullopt;
        return distance_opt.value() + (new_pos.offset_mm - old_pos.offset_mm);
    }

    void check_scheduled_wakeups() {
        if (blocked_task.has_value()) {
            // TODO: multiple task support
            wakeup_t& wake = blocked_task.value();

            // look up associated train descriptor
            const train_descriptor_t* td_opt = descriptor_for(wake.train);
            assert(td_opt != nullptr);
            const train_descriptor_t& td = *td_opt;

            // use td.pos, td.pos_observed_at + Clock::Time, and td.velocity to
            // extrapolate the train's current position

            int dt = Clock::Time(clock) - td.pos_observed_at;
            assert(dt >= 0);

            // TODO if our velocity measurements are way off, this distance
            // could be WAY off - we should cap this at 2 sensor distances from
            // the last observed sensor.
            int distance_travelled_mm = td.velocity * dt / TICKS_PER_SEC;

            Marklin::track_pos_t new_pos = td.pos;
            new_pos.offset_mm += distance_travelled_mm;
            // FIXME: implement offset normalization
            // (i.e: if offset is > next sensor)
            // required to be robust against broken sensors!

            auto distance_to_target_opt = distance_between(new_pos, wake.pos);
            if (!distance_to_target_opt.has_value()) {
                // wake up the task, and remove it from the blocked list
                log_line(uart,
                         VT_YELLOW "WARNING" VT_NOFMT
                                   " wake_at_pos: no route from %c%u@%d to "
                                   "%c%u@%d for train %u",
                         new_pos.sensor.group, new_pos.sensor.idx,
                         new_pos.offset_mm, wake.pos.sensor.group,
                         wake.pos.sensor.idx, wake.pos.offset_mm, td.id);
                Res res = {.tag = MsgTag::WakeAtPos,
                           .wake_at_pos = {.success = false}};
                Reply(wake.tid, (char*)&res, sizeof(res));
                blocked_task = std::nullopt;
                return;
            }

            int distance_to_target_mm = distance_to_target_opt.value();
            int ticks_until_target =
                (TICKS_PER_SEC * distance_to_target_mm) / td.velocity;

            log_line(
                uart,
                "train %d velocity=%dmm/s distance to target=%dmm "
                "(%c%u@%d->%c%u@%d) ticks_until_target=%d",
                td.id, td.velocity, distance_to_target_mm, new_pos.sensor.group,
                new_pos.sensor.idx, new_pos.offset_mm, wake.pos.sensor.group,
                wake.pos.sensor.idx, wake.pos.offset_mm, ticks_until_target);

            // If we won't tick again until after the deadline, delay until the
            // exact moment that we wish to respond.
            if (ticks_until_target <= max_tick_delay) {
                if (ticks_until_target > 0)
                    Clock::Delay(clock, ticks_until_target);

                // wake up the task, and remove it from the blocked list
                Res res = {.tag = MsgTag::WakeAtPos,
                           .wake_at_pos = {.success = true}};
                Reply(wake.tid, (char*)&res, sizeof(res));
                blocked_task = std::nullopt;
            }
        }
    }

    void interpolate_acceleration(int now) {
        for (train_descriptor_t& td : trains) {
            if (td.id <= 0) continue;
            if (!td.accelerating) continue;

            if (td.old_speed == td.speed) {
                log_line(uart,
                         VT_YELLOW "WARNING" VT_NOFMT
                                   " interpolate_acceleration: "
                                   "old_speed=new_speed=%u for train %u",
                         td.speed, td.id);
                td.accelerating = false;
                continue;
            }

            int steady_state_velocity =
                Calibration::expected_velocity(td.id, td.speed);
            int how_long_ive_been_accelerating = now - td.speed_changed_at;
            int how_long_it_takes_to_accelerate =
                Calibration::acceleration_time(td.id, td.old_velocity,
                                               td.speed);

            assert(how_long_ive_been_accelerating >= 0);

            if (how_long_ive_been_accelerating >=
                how_long_it_takes_to_accelerate) {
                td.velocity = steady_state_velocity;
                td.accelerating = false;
                log_line(uart, "train %u finished accelerating", td.id);
            } else {
                // interpolate the train's velocity to be between its old
                // velocity and its expected steady-state velocity.
                td.velocity = td.old_velocity +
                              (how_long_ive_been_accelerating *
                               (steady_state_velocity - td.old_velocity)) /
                                  how_long_it_takes_to_accelerate;
            }
            Ui::render_train_descriptor(uart, td);
        }
    }

   public:
    // disallow copying, moving, or default-constructing
    TrackOracleImpl() = delete;
    TrackOracleImpl(const TrackOracleImpl&) = delete;
    TrackOracleImpl(TrackOracleImpl&&) = delete;
    TrackOracleImpl& operator=(const TrackOracleImpl&) = delete;
    TrackOracleImpl& operator=(TrackOracleImpl&&) = delete;

    TrackOracleImpl(int uart_tid, int clock_tid, Marklin::Track track_id)
        : track(track_id),
          uart{uart_tid},
          clock{clock_tid},
          marklin(uart_tid),
          last_ticked_at{-1},
          max_tick_delay{0} {
        memset(trains, 0, sizeof(train_descriptor_t) * MAX_TRAINS);

        // TODO: actually have different inits for different tracks
        log_line(uart, "Initializing Track...");

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

        make_loop();

        log_line(uart, "Track has been initialized!");
    }

    void make_loop() {
        track.make_loop();
        log_line(uart, "Setting switch positions...");
        marklin.update_branches(track.get_branches(), TrackGraph::BRANCHES_LEN);
        marklin.flush();
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
                    .offset_mm = Calibration::stopping_distance(id, 8)},
            .pos_observed_at = now,
            .speed_changed_at = now,
            .accelerating = false,
            .old_speed = 0,
            .old_velocity = 0,
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

    bool set_train_speed(uint8_t id, uint8_t speed) {
        // send marklin command
        auto train = Marklin::TrainState(id);
        train.set_speed(speed);
        train.set_light(true);
        marklin.update_train(train);

        // update train state (if the train is registered)
        train_descriptor_t* td_opt = descriptor_for(id);
        if (td_opt == nullptr) return false;
        train_descriptor_t& td = *td_opt;

        uint8_t old_speed = td.speed;
        td.speed = speed;
        td.old_speed = old_speed;
        td.old_velocity = td.velocity;
        td.lights = true;

        int now = Clock::Time(clock);
        if (old_speed == 0 && speed > 0) {
            td.pos_observed_at = now;
        }
        if (old_speed != speed) {
            td.accelerating = true;
            td.speed_changed_at = now;
        }
        Ui::render_train_descriptor(uart, td);

        return true;
    }

    bool reverse_train(uint8_t id) {
        // send the reverse command
        auto train = Marklin::TrainState(id);
        train.set_speed(15);
        train.set_light(true);
        marklin.update_train(train);

        // update the train state (if the train is registered)
        train_descriptor_t* td_opt = descriptor_for(id);
        if (td_opt == nullptr) return false;
        train_descriptor_t& td = *td_opt;

        if (td.speed != 0) {
            log_warning(uart,
                        "attempted to reverse train with non zero speed!");
            return false;
        }

        const Marklin::track_pos_t old_pos = td.pos;
        td.pos.sensor = track.invert_sensor(old_pos.sensor);
        td.pos.offset_mm = -old_pos.offset_mm;

        auto next_sensor_opt = track.next_sensor(td.pos.sensor);
        if (next_sensor_opt.has_value()) {
            auto [sensor, distance] = next_sensor_opt.value();
            td.has_next_sensor = true;
            td.next_sensor = sensor;
            td.next_sensor_time = INT_MAX;  // speed is zero after all
        } else {
            td.has_next_sensor = false;
        }

        Ui::render_train_descriptor(uart, td);

        return true;
    }

    void set_branch_dir(uint8_t id, Marklin::BranchDir dir) {
        track.set_branch_dir(id, dir);
        marklin.update_branch(id, dir);

        // re-calculate next sensor for all the trains
        int now = Clock::Time(clock);
        for (train_descriptor_t& td : trains) {
            if (td.id == 0) continue;

            auto next_sensor_opt = track.next_sensor(td.pos.sensor);
            if (next_sensor_opt.has_value()) {
                auto [sensor, distance] = next_sensor_opt.value();
                td.has_next_sensor = true;
                td.next_sensor = sensor;
                td.next_sensor_time =
                    now + ((TICKS_PER_SEC * distance) / td.velocity);
            } else {
                td.has_next_sensor = false;
            }
        }
    }

    void update_sensors() {
        Marklin::SensorData sensor_data;
        marklin.query_sensors(sensor_data.raw);

        int now = Clock::Time(clock);
        while (auto sensor_opt = sensor_data.next_sensor()) {
            Marklin::sensor_t sensor = sensor_opt.value();

            train_descriptor_t* td_opt = attribute_sensor(sensor);
            if (td_opt == nullptr) {
                log_line(uart,
                         "could not attribute sensor %c%hhu to any train, "
                         "treating as spurious.",
                         sensor.group, sensor.idx);
                continue;
            }
            train_descriptor_t& td = *td_opt;

            if (Marklin::sensor_eq(sensor, td.pos.sensor)) {
                // The sensor was triggered more than once - simply update
                // pos_observed_at.
                td.pos_observed_at = now;
                continue;
            }

            const Marklin::track_pos_t old_pos = td.pos;
            const Marklin::track_pos_t new_pos = {
                .sensor = sensor,
                .offset_mm =
                    0 /* TODO account for velocity and expected sensor delay */
            };
            auto distance_opt = distance_between(old_pos, new_pos);
            if (!distance_opt.has_value()) {
                log_line(uart,
                         VT_YELLOW
                         "WARNING" VT_NOFMT
                         " cannot calculate distance between %c%u@%d and "
                         "%c%u@%d despite being attributed to train %d",
                         old_pos.sensor.group, old_pos.sensor.idx,
                         old_pos.offset_mm, new_pos.sensor.group,
                         new_pos.sensor.idx, new_pos.offset_mm, td.id);
                continue;
            }
            int distance_mm = distance_opt.value();
            int dt_ticks = now - td.pos_observed_at;
            if (dt_ticks <= 0) continue;
            int new_velocity_mmps = (TICKS_PER_SEC * distance_mm) / dt_ticks;

            int how_long_ive_been_accelerating = now - td.speed_changed_at;
            int acceleration_time = Calibration::acceleration_time(
                td.id, td.old_velocity, td.speed);

            if (how_long_ive_been_accelerating < acceleration_time) {
                // If the train is accelerating, we are super uncertain of its
                // velocity, so we set our velocity to the ovserved velocity
                // directly.
                td.velocity = new_velocity_mmps;

                // given that we've observed the train going at speed v1 at time
                // t1, and we expect the train to reach speed v2 at time t2, we
                // can update old_velocity such that as we interpolate
                // velocities during acceleration, the velocities  are
                // interpolated to be between v1 and v2
                int v1 = new_velocity_mmps;
                int t1 = how_long_ive_been_accelerating;
                int v2 = Calibration::expected_velocity(td.id, td.speed);
                int t2 = acceleration_time;

                int v0 = v1 - ((v2 - v1) * t1) / (t2 - t1);
                log_line(uart,
                         "interpolating v0: v1=%d v2=%d t1=%d t2=%d -> v0=%d",
                         v1, v2, t1, t2, v0);
                td.old_velocity = std::min(1000, std::max(v0, 0));
            } else {
                // Once a train has been cruising at the same speed level for a
                // time, we assume the train's velocity is close to constant. So
                // we use an exponentially weighted moving average to update our
                // velocity, smoothing out inconsitencies.
                td.velocity = ewma4(td.velocity, new_velocity_mmps);
            }
            td.pos = new_pos;
            td.pos_observed_at = now;

            // calculate time and distance error
            if (td.has_next_sensor &&
                Marklin::sensor_eq(sensor, td.next_sensor)) {
                td.has_error = true;
                td.time_error = now - td.next_sensor_time;
                td.distance_error = td.velocity * td.time_error / TICKS_PER_SEC;
            } else {
                // TODO if we miss a sensor, we could probably still extrapolate
                // this error
                td.has_error = false;
            }

            auto next_sensor_opt = track.next_sensor(sensor);
            if (next_sensor_opt.has_value()) {
                auto [sensor, distance] = next_sensor_opt.value();
                td.has_next_sensor = true;
                td.next_sensor = sensor;
                td.next_sensor_time =
                    now + ((TICKS_PER_SEC * distance) / td.velocity);
            } else {
                td.has_next_sensor = false;
            }

            Ui::render_train_descriptor(uart, td);
            log_line(uart,
                     "observed train %d from %c%02hhu->%c%02hhu dx=%dmm "
                     "dt=%d.%02ds dx/dt=%dmm/s",
                     td.id, old_pos.sensor.group, old_pos.sensor.idx,
                     new_pos.sensor.group, new_pos.sensor.idx, distance_mm,
                     dt_ticks / TICKS_PER_SEC, dt_ticks % TICKS_PER_SEC,
                     new_velocity_mmps);
        }
        tick();
    }

    void wake_at_pos(int tid, uint8_t train, Marklin::track_pos_t pos) {
        if (blocked_task.has_value()) {
            panic("only one task can block on a train (at the moment)");
        }

        if (descriptor_for(train) == nullptr) {
            log_line(uart,
                     VT_YELLOW "WARNING" VT_NOFMT
                               " wake_at_pos: uncalibrated train %u",
                     train);
            Res res = {.tag = MsgTag::WakeAtPos,
                       .wake_at_pos = {.success = false}};
            Reply(tid, (char*)&res, sizeof(res));
            return;
        }

        auto npos = track.normalize(pos);
        log_line(uart, "normalized %c%u@%d to %c%u@%d", pos.sensor.group,
                 pos.sensor.idx, pos.offset_mm, npos.sensor.group,
                 npos.sensor.idx, npos.offset_mm);
        blocked_task = {.tid = tid, .train = train, .pos = npos};
    }

    train_descriptor_t* query_train(uint8_t train) {
        return descriptor_for(train);
    }

    void tick() {
        int now = Clock::Time(clock);
        if (last_ticked_at == -1) {
            last_ticked_at = now;
        } else {
            int delay = now - last_ticked_at;
            if (delay > max_tick_delay && delay < 100) {
                max_tick_delay = delay;
                log_line(uart, "max tick delay updated to %d ticks", delay);
            }
            last_ticked_at = now;
        }

        interpolate_acceleration(now);
        check_scheduled_wakeups();
    }

    Marklin::track_pos_t normalize(const Marklin::track_pos_t& pos) {
        return track.normalize(pos);
    }
};

void TrackOracleTickerTask() {
    int tid = MyParentTid();
    assert(tid >= 0);
    int clock = WhoIs(Clock::SERVER_ID);
    assert(clock >= 0);
    const Req req = {.tag = MsgTag::Tick, .tick = {}};
    while (true) {
        Send(tid, (char*)&req, sizeof(req), nullptr, 0);
        Clock::Delay(clock, 1);
    }
}

void TrackOracleTask() {
    int nsres = RegisterAs(TRACK_ORACLE_TASK_ID);
    assert(nsres >= 0);

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

    Create(0, TrackOracleTickerTask);

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
                oracle.wake_at_pos(tid, req.wake_at_pos.id,
                                   req.wake_at_pos.pos);
                continue;  // IMPORTANT! MUST NOT IMMEDIATELY RESPOND TO TASK!
            } break;
            case MsgTag::SetTrainSpeed: {
                res.set_train_speed.success = oracle.set_train_speed(
                    req.set_train_speed.id, req.set_train_speed.speed);
            } break;
            case MsgTag::SetTrainLight: {
                panic("TrackOracle: SetTrainLight message unimplemented");
            } break;
            case MsgTag::ReverseTrain: {
                res.reverse_train.success =
                    oracle.reverse_train(req.reverse_train.id);
            } break;
            case MsgTag::SetBranchDir: {
                oracle.set_branch_dir(req.set_branch_dir.id,
                                      req.set_branch_dir.dir);
            } break;
            case MsgTag::QueryTrain: {
                train_descriptor_t* td = oracle.query_train(req.query_train.id);
                if (td != nullptr) {
                    res.query_train.valid = true;
                    res.query_train.desc = *td;
                } else {
                    res.query_train.valid = false;
                }
            } break;
            case MsgTag::QueryBranch: {
                panic("TrackOracle: QueryBranch message unimplemented");
            } break;
            case MsgTag::UpdateSensors: {
                oracle.update_sensors();
            } break;
            case MsgTag::Tick: {
                oracle.tick();
            } break;
            case MsgTag::Normalize: {
                res.normalize = oracle.normalize(req.normalize);
            } break;
            case MsgTag::MakeLoop: {
                oracle.make_loop();
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

TrackOracle::TrackOracle() {
    int tid = WhoIs(TRACK_ORACLE_TASK_ID);
    if (tid < 0) {
        panic(
            "TrackOracle::TrackOracle - oracle task not running (WhoIs "
            "returned %d). Did you forget to use the task spawning constructor "
            "first?",
            tid);
    }
    this->tid = tid;
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
bool TrackOracle::set_train_speed(uint8_t id, uint8_t speed) {
    Req req = {.tag = MsgTag::SetTrainSpeed,
               .set_train_speed = {.id = id, .speed = speed}};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return res.set_train_speed.success;
}
/// Update a train's lights
bool TrackOracle::set_train_light(uint8_t id, bool active) {
    Req req = {.tag = MsgTag::SetTrainSpeed,
               .set_train_light = {.id = id, .active = active}};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return res.set_train_light.success;
}

/// Reverse a train's direction (via speed 15)
bool TrackOracle::reverse_train(uint8_t id) {
    Req req = {.tag = MsgTag::ReverseTrain, .reverse_train = {.id = id}};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return res.reverse_train.success;
}

/// Update a branch's direction
void TrackOracle::set_branch_dir(uint8_t id, Marklin::BranchDir dir) {
    Req req = {.tag = MsgTag::SetBranchDir,
               .set_branch_dir = {.id = id, .dir = dir}};
    send_with_assert_empty_response(this->tid, req);
}

void TrackOracle::update_sensors() {
    Req req = {.tag = MsgTag::UpdateSensors, .update_sensors = {}};
    send_with_assert_empty_response(this->tid, req);
}

void TrackOracle::make_loop() {
    Req req = {.tag = MsgTag::MakeLoop, .make_loop = {}};
    send_with_assert_empty_response(this->tid, req);
}

/// Return a particular train's current state
std::optional<train_descriptor_t> TrackOracle::query_train(uint8_t id) {
    Req req = {.tag = MsgTag::QueryTrain, .query_train = {.id = id}};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    if (res.query_train.valid) {
        return res.query_train.desc;
    } else {
        return std::nullopt;
    }
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
bool TrackOracle::wake_at_pos(uint8_t train_id, Marklin::track_pos_t pos) {
    Req req = {.tag = MsgTag::WakeAtPos,
               .wake_at_pos = {.id = train_id, .pos = pos}};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return res.wake_at_pos.success;
}

Marklin::track_pos_t TrackOracle::normalize(const Marklin::track_pos_t& pos) {
    Req req = {.tag = MsgTag::Normalize, .normalize = pos};
    Res res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) panic("truncated response");
    if (res.tag != req.tag) panic("mismatched response kind");
    return res.normalize;
}
