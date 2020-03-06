#include <cctype>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "calibration.h"
#include "cmd.h"
#include "marklin.h"
#include "sysperf.h"
#include "track_graph.h"
#include "track_oracle.h"
#include "ui.h"

static inline void print_help(const int uart) {
    // clang-format off
    log_line(uart,
        "t1 commands:" ENDL
        "  addtr <train-id>                - register a train with the track" ENDL
        "  route <train> <sensor> <offset> - route train to sensor with given offset" ENDL
        "  q                               - quit" ENDL
        ENDL
        "debug commands:" ENDL
        "  tr <train> <speed>   - set a train to a certain speed" ENDL
        "  sw <branch> <c|s>    - set a branch to be (c)urved or (s)traight" ENDL
        "  rv <train>           - reverse a train (MUST BE AT SPEED ZERO!)" ENDL
        "  n <sensor> <offset>  - print the position, normalized" ENDL
    );
    // clang-format on
}

static inline Marklin::Track query_user_for_track(const int uart) {
    while (true) {
        log_line(uart,
                 VT_GREEN "Enter the track you'll be using (A or B)" VT_NOFMT);
        char buf[2];
        Ui::prompt_user(uart, buf, sizeof(buf));

        switch (tolower(buf[0])) {
            case 'a':
                return Marklin::Track::A;
            case 'b':
                return Marklin::Track::B;
            default:
                log_line(uart, VT_RED "Invalid track value." VT_NOFMT);
        }
    }
}

static inline bool is_valid_train(const uint8_t no) {
    for (uint8_t valid_id : Marklin::VALID_TRAINS) {
        if (no == valid_id) {
            return true;
        }
    }
    return false;
}

static inline bool is_valid_sensor(const Marklin::sensor_t& sensor) {
    if (sensor.group < 'A' || sensor.group > 'E') {
        return false;
    }
    if (sensor.idx > 16) return false;
    return true;
}

static inline void wait_for_enter(const int uart) {
    char dummy;
    Ui::prompt_user(uart, &dummy, sizeof(dummy));
}

static void do_route_cmd(const int uart,
                         const int clock,
                         TrackOracle& track_oracle,
                         const TrackGraph& track_graph,
                         const Command::route_t& cmd) {
    const Marklin::sensor_t sensor = {.group = cmd.sensor_group,
                                      .idx = (uint8_t)cmd.sensor_idx};
    const uint8_t train = (uint8_t)cmd.train;

    if (!is_valid_train(train)) {
        log_line(uart, VT_RED "Invalid train id." VT_NOFMT);
        return;
    }

    if (!is_valid_sensor(sensor)) {
        log_line(uart, VT_RED "Invalid sensor." VT_NOFMT);
        return;
    }

    // TODO use this lol
    (void)track_graph;

    log_line(uart,
             VT_CYAN "Routing train %u to sensor %c%u + offset %d" VT_NOFMT,
             train, sensor.group, sensor.idx, cmd.offset);

    if (cmd.dry_run) {
        log_line(uart, VT_YELLOW
                 "Found --dry-run flag, not actually routing..." VT_NOFMT);
        return;
    }

    auto td_opt = track_oracle.query_train(train);
    if (!td_opt.has_value()) {
        log_line(
            uart,
            VT_RED
            "train %u is not calibrated. Please run 'addtr %u' first." VT_NOFMT,
            train, train);
        return;
    }

    track_oracle.set_train_speed(train, 8);
    int stop_at_offset = cmd.offset - Calibration::stopping_distance(train, 8);
    const Marklin::track_pos_t send_stop_at_pos = {.sensor = sensor,
                                                   .offset_mm = stop_at_offset};
    log_line(uart,
             VT_CYAN
             "Waiting for train %u to reach sensor %c%u%c%dmm ..." VT_NOFMT,
             train, sensor.group, sensor.idx, stop_at_offset < 0 ? '-' : '+',
             std::abs(stop_at_offset));
    if (!track_oracle.wake_at_pos(train, send_stop_at_pos)) {
        log_line(uart,
                 VT_RED "Routing failed :'(" VT_NOFMT
                        " stopping train %d in place.",
                 train);
        track_oracle.set_train_speed(train, 0);
    }
    log_line(uart,
             VT_CYAN
             "Sending speed=0 to train %u. Waiting for train to "
             "stop..." VT_NOFMT,
             train);
    track_oracle.set_train_speed(train, 0);
    Clock::Delay(clock, Calibration::stopping_time(train, 8));
    log_line(uart, VT_CYAN "Stopped! (hopefully)" VT_NOFMT);
}

struct CmdTaskCfg {
    Marklin::Track track;
};

static void CmdTask() {
    CmdTaskCfg cfg;
    {
        int tid;
        int n = Receive(&tid, (char*)&cfg, sizeof(cfg));
        assert(n == sizeof(cfg));
        Reply(tid, nullptr, 0);
    }
    int uart = WhoIs(Uart::SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);
    assert(uart >= 0);
    assert(clock >= 0);

    // This looks up the oracle task in the nameserver.
    TrackOracle track_oracle = TrackOracle();

    // We keep a copy track of the track graph ourselves so we can query it when
    // routing.
    TrackGraph track_graph(cfg.track);

    log_line(uart, VT_YELLOW "Ready to accept commands!" VT_NOFMT);
    print_help(uart);

    char line[80];
    while (true) {
        Ui::prompt_user(uart, line, sizeof(line));

        std::optional<Command> cmd_opt = Command::from_string(line);
        if (!cmd_opt.has_value()) {
            log_line(uart, VT_RED "Unrecognized command." VT_NOFMT);
            continue;
        }

        const Command& cmd = cmd_opt.value();

        switch (cmd.kind) {
            case Command::ADDTR: {
                const uint8_t train = (uint8_t)cmd.addtr.no;
                if (!is_valid_train(train)) {
                    log_line(uart, VT_RED "Invalid train id." VT_NOFMT);
                    continue;
                }

                log_line(
                    uart, VT_YELLOW
                    "Place the train somewhere on the track." VT_NOFMT ENDL
                    "It's okay if the train is running, we'll send a stop "
                    "command." ENDL VT_GREEN
                    "Press [ENTER] once the train is on the track." VT_NOFMT);
                wait_for_enter(uart);

                // register the train with the oracle
                track_oracle.calibrate_train(train);

                log_line(uart,
                         VT_GREEN "Press [ENTER] to start the train" VT_NOFMT);
                wait_for_enter(uart);

                track_oracle.set_train_speed(train, 14);
            } break;
            case Command::NORMALIZE: {
                Marklin::track_pos_t pos = {
                    .sensor =
                        {
                            .group = cmd.normalize.sensor_group,
                            .idx = (uint8_t)cmd.normalize.sensor_idx,
                        },
                    .offset_mm = cmd.normalize.offset};
                Marklin::track_pos_t npos = track_oracle.normalize(pos);
                log_line(uart, "Position %c%u@%d normalizes to %c%u@%d.",
                         pos.sensor.group, pos.sensor.idx, pos.offset_mm,
                         npos.sensor.group, npos.sensor.idx, npos.offset_mm);
            } break;
            case Command::HELP: {
                print_help(uart);
            } break;
            case Command::GO: {
                // IMPROVEMENT: actually implement go command
                log_line(uart, VT_RED "Invalid command." VT_NOFMT);
            } break;
            case Command::LIGHT: {
                // IMPROVEMENT: actually implement light command
                log_line(uart, VT_RED "Invalid command." VT_NOFMT);
            } break;
            case Command::Q: {
                Uart::Putstr(uart, COM2, VT_RESET);
                Uart::Flush(uart, COM2);
                Shutdown();
            } break;
            case Command::ROUTE: {
                do_route_cmd(uart, clock, track_oracle, track_graph, cmd.route);
            } break;
            case Command::RV: {
                track_oracle.reverse_train((uint8_t)cmd.rv.no);
                log_line(uart, VT_CYAN "Manually reversed train %u" VT_NOFMT,
                         cmd.rv.no);
            } break;
            case Command::STOP: {
                // IMPROVEMENT: actually implement stop command
                log_line(uart, VT_RED "Invalid command." VT_NOFMT);
            } break;
            case Command::SW: {
                track_oracle.set_branch_dir((uint8_t)cmd.sw.no, cmd.sw.dir);
                log_line(uart, VT_CYAN "Manually set branch %u to %s" VT_NOFMT,
                         cmd.sw.no,
                         cmd.sw.dir == Marklin::BranchDir::Straight ? "straight"
                                                                    : "curved");
            } break;
            case Command::TR: {
                track_oracle.set_train_speed((uint8_t)cmd.tr.no,
                                             (uint8_t)cmd.tr.speed);
                log_line(uart,
                         VT_CYAN "Manually set train %u to speed %u" VT_NOFMT,
                         cmd.tr.no, cmd.tr.speed);
            } break;
            default:
                panic("somehow parsed an invalid command!");
        }
    }
}

static void t1_main(int clock, int uart) {
    (void)clock;

    // determine which track to use
    Marklin::Track track_id = query_user_for_track(uart);

    // create the track oracle (which also instantiates the track)
    TrackOracle track_oracle = TrackOracle(track_id);

    // CmdTask has higher priority than t1_main so that oracle commands it sends
    // trump sensor queries.
    int cmdtask = Create(1, CmdTask);
    {
        CmdTaskCfg cfg = {.track = track_id};
        int n = Send(cmdtask, (char*)&cfg, sizeof(cfg), nullptr, 0);
        assert(n == 0);
    }

    // This task is now the update_sensors task
    while (true) {
        track_oracle.update_sensors();
    }
}

void FirstUserTask() {
    int clock = Create(1000, Clock::Server);
    int uart = Create(1000, Uart::Server);

    assert(clock >= 0);
    assert(uart >= 0);

    // clear the terminal
    Uart::Putstr(uart, COM2, VT_CLEAR);

    // read the term's dimensions
    Uart::Drain(uart, COM2);
    TermSize term_size = query_term_size(
        &uart,
        [](void* d) {
            const int uart = *(int*)d;
            return (char)Uart::Getc(uart, COM2);
        },
        [](void* d, const char* s) { Uart::Putstr(*(int*)d, COM2, s); });
    assert(term_size.success);

    Ui::render_initial_screen(uart, term_size);

    // spawn the perf task
    {
        int tid = Create(0, SysPerf::Task);
        SysPerf::TaskCfg cfg = {.term_size = term_size};
        Send(tid, (char*)&cfg, sizeof(cfg), nullptr, 0);
    }

    t1_main(clock, uart);
}
