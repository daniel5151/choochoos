#include "track_oracle.h"

#include <cstring>
#include <initializer_list>
#include <optional>

#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

class TrackOracleImpl {
   private:
    const Marklin::Track track;
    const int uart;
    const int clock;
    const Marklin::Controller marklin;

    // TODO: enumerate state (i.e: train descriptors, which track we're on, etc)

   public:
    // disallow copying, moving, or default-constructing
    TrackOracleImpl() = delete;
    TrackOracleImpl(const TrackOracleImpl&) = delete;
    TrackOracleImpl(TrackOracleImpl&&) = delete;
    TrackOracleImpl& operator=(const TrackOracleImpl&) = delete;
    TrackOracleImpl& operator=(TrackOracleImpl&&) = delete;

    TrackOracleImpl(int uart_tid, int clock_tid, Marklin::Track track_id)
        : track{track_id}, uart{uart_tid}, clock{clock_tid}, marklin(uart_tid) {
        // TODO: actually have different inits for different tracks
        Uart::Printf(uart, COM2, "Initializing Track A..." ENDL);

        // ensure the track is on
        marklin.send_go();

        Uart::Printf(uart, COM2, "Stopping all trains..." ENDL);
        auto train = Marklin::TrainState(0);
        train.set_speed(0);
        train.set_light(false);
        for (uint8_t id : Marklin::VALID_TRAINS) {
            train.set_id(id);
            marklin.update_train(train);
        }

        marklin.flush();

        // set all the branches to curved
        Marklin::BranchState branches[sizeof(Marklin::VALID_SWITCHES)];
        for (size_t i = 0; auto& b : branches) {
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

        Uart::Printf(uart, COM2, "Setting switch positions..." ENDL);
        marklin.update_branches(branches, sizeof(Marklin::VALID_SWITCHES));
        marklin.flush();

        Uart::Printf(uart, COM2, "Track has been initialized!" ENDL);
    }

    void calibrate_train(uint8_t id) {
        Uart::Printf(uart, COM2, "Stopping train %hhu..." ENDL, id);
        set_train_speed(id, 0);
        Clock::Delay(clock, 200);  // make sure it's slowed down

        Uart::Printf(uart, COM2, "Waiting for train to hit a sensor..." ENDL);

        // give it some gas
        set_train_speed(id, 7);

        // loop until a sensor is hit
        Marklin::SensorData sensor_data;
        marklin.query_sensors(sensor_data.raw);  // clear residual sensor data

        while (true) {
            marklin.query_sensors(sensor_data.raw);
            Clock::Delay(clock, 25);

            auto sensor_opt = sensor_data.next_sensor();
            if (sensor_opt.has_value()) {
                auto sensor = sensor_opt.value();

                Uart::Printf(uart, COM2, "Train hit sensor %c%hhu!" ENDL,
                             sensor.group, sensor.idx);
                break;
            }
        }

        set_train_speed(id, 0);  // stop the train

        Uart::Printf(uart, COM2, "Done calibrating train %hhu..." ENDL, id);
    }

    void set_train_speed(uint8_t id, uint8_t speed) {
        // TODO: lookup train descriptor
        auto train = Marklin::TrainState(id);
        train.set_speed(speed);
        train.set_light(true);
        marklin.update_train(train);
    }

    void update_sensors() { panic("unimplemented"); }
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

    Uart::Printf(uart, COM2, "Spawned TrackOracleTask!" ENDL);

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
