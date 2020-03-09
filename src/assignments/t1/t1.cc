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
        "  addtr <train>                   - register a train with the track" ENDL
        "  route <train> <sensor> <offset> - route train to given sensor + offset" ENDL
        "  tr <train> <speed>              - set a train to a certain speed" ENDL
        "  q                               - quit" ENDL
        ENDL
        "debug commands:" ENDL
        "  help                   - prints this help message" ENDL
        "  sw <branch> <c|s>      - set a branch to be (c)urved or (s)traight" ENDL
        "  rv <train>             - reverse a train (MUST BE AT SPEED ZERO!)" ENDL
        "  n <sensor> <offset>    - print the position, normalized" ENDL
        "  path <sensor> <sensor> - calculate route between two sensors" ENDL
    );
    // clang-format on
}

static inline Marklin::Track query_user_for_track(const int uart) {
    while (true) {
        log_prompt(uart, "Enter the track you'll be using (A or B)");
        char buf[2];
        Ui::prompt_user(uart, buf, sizeof(buf));

        switch (tolower(buf[0])) {
            case 'a':
                return Marklin::Track::A;
            case 'b':
                return Marklin::Track::B;
            default:
                log_error(uart, "Invalid track value.");
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
    Marklin::sensor_t sensor = {.group = cmd.sensor_group,
                                .idx = (uint8_t)cmd.sensor_idx};
    const uint8_t train = (uint8_t)cmd.train;

    if (!is_valid_train(train)) {
        log_error(uart, "Invalid train id.");
        return;
    }

    if (!is_valid_sensor(sensor)) {
        log_error(uart, "Invalid sensor.");
        return;
    }

    auto td_opt = track_oracle.query_train(train);
    if (!td_opt.has_value()) {
        log_error(uart,
                  "train %u is not calibrated. Please run 'addtr %u' first.",
                  train, train);
        return;
    }
    const train_descriptor_t& td = td_opt.value();

    log_success(uart, "Routing train %u (%c%u) to sensor %c%u + %dmm", train,
                td.pos.sensor.group, td.pos.sensor.idx, sensor.group,
                sensor.idx, cmd.offset);

    // find the shortest path
    constexpr size_t MAX_PATH_LEN = 64;  // 64 is overkill, but it's fine
    const track_node* path[MAX_PATH_LEN];
    size_t distance = 0;
    int path_len = 0;

    path_len = track_graph.shortest_path(td.pos.sensor, sensor, path,
                                         MAX_PATH_LEN, distance);
    if (path_len < 0) {
        // that's okay, all this means is that we might be stopped at a terminal
        // of the track. if our speed is zero, then we could try to reverse, and
        // then route.
        if (td.speed != 0) {
            log_error(uart,
                      "could not find path, even though the train was running "
                      "(i.e: it's assumed to be in a loop)");
            return;
        }

        track_oracle.reverse_train(train);

        td_opt = track_oracle.query_train(train);
        assert(td_opt.has_value());
        const train_descriptor_t& td = td_opt.value();

        path_len = track_graph.shortest_path(td.pos.sensor, sensor, path,
                                             MAX_PATH_LEN, distance);

        if (path_len < 0) {
            log_error(uart,
                      "could not find path, even after reversing the train!");
            return;
        }
    }

    // FIXME: handle zero-len paths
    if (path_len == 0) {
        log_error(uart, "zero len paths not supported :/");
        return;
    }

    // FIXME: instead of doing "JIT" branch updates, we just set all of them,
    // and then wait for the train to hit certain checkpoints.

    for (size_t i = 0; i < (size_t)path_len; i++) {
        assert(path[i] != nullptr);
        const track_node& n = *path[i];
        log_line(uart, "%s", n.name);

        switch (n.type) {
            case NODE_NONE:
                panic("unexpected NODE_NONE in path!");
            case NODE_SENSOR: {
                // wait until the train hits the sensor
                // if (!cmd.dry_run) {
                //     const Marklin::track_pos_t s = {
                //         .sensor =
                //             {.group = (char)((n.num / 16) + 'A'),
                //              .idx = (uint8_t)((n.num % 16) + 1)},
                //         .offset_mm = 0};

                //     bool ok = track_oracle.wake_at_pos(train, s);
                //     if (!ok) {
                //         log_line(
                //             uart, VT_RED
                //             "wake_at_pos before sensor failed!" VT_NOFMT);
                //     }
                // }
            } break;
            case NODE_BRANCH: {
                // determine what direction it needs to be switched to
                assert(i + 1 < (size_t)path_len);
                const track_node& next_n = *path[i + 1];

                std::optional<Marklin::BranchDir> dir = std::nullopt;

                if (n.edge[DIR_STRAIGHT].dest == &next_n) {
                    log_line(uart, "set branch %d to straight", n.num);
                    dir = Marklin::BranchDir::Straight;
                } else if (n.edge[DIR_CURVED].dest == &next_n) {
                    log_line(uart, "set branch %d to curved", n.num);
                    dir = Marklin::BranchDir::Curved;
                } else {
                    panic("branch doesn't lead to next node in path!");
                }

                if (!cmd.dry_run) {
                    // switch the branch
                    track_oracle.set_branch_dir((uint8_t)n.num, dir.value());
                }
            } break;
            case NODE_MERGE: {
                // we don't actually care about merges...
                continue;
            } break;
            case NODE_ENTER:
                panic("unexpected NODE_ENTER in path!");
            case NODE_EXIT:
                panic("unexpected NODE_EXIT in path!");
            default:
                panic("unknown track_node.type in path!");
        }
    }

    if (cmd.dry_run) {
        log_warning(uart, "Found --dry-run flag, not actually routing...");
        return;
    }

    int stop_at_offset = cmd.offset - Calibration::stopping_distance(train, 8);
    Marklin::track_pos_t send_stop_at_pos = {.sensor = sensor,
                                             .offset_mm = stop_at_offset};

    // TODO: use velocity-change data
    Marklin::track_pos_t send_slow_at_pos = send_stop_at_pos;
    send_slow_at_pos.offset_mm -= 1000;

    ///////////////////////////////

    if (distance > std::abs(send_slow_at_pos.offset_mm)) {
        // let 'er eat at 14
        track_oracle.set_train_speed(train, 14);

        bool ok = track_oracle.wake_at_pos(train, send_slow_at_pos);
        if (!ok) {
            log_error(uart, "wake_at_pos for slow-down failed unexpectedly!");
            return;
        }
    }

    track_oracle.set_train_speed(train, 8);

    log_success(uart, "Waiting for train %u to reach sensor %c%u %c %dmm ...",
                train, sensor.group, sensor.idx, stop_at_offset < 0 ? '-' : '+',
                std::abs(stop_at_offset));

    bool ok = track_oracle.wake_at_pos(train, send_stop_at_pos);

    if (!ok) {
        log_error(uart, "wake_at_pos for stop failed unexpectedly!");
        return;
    }

    log_success(uart,
                "Sending speed=0 to train %u. Waiting for train to "
                "stop...",
                train);
    track_oracle.set_train_speed(train, 0);

    Clock::Delay(clock, Calibration::stopping_time(train, 8));
    log_success(uart, "Stopped!");
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

    log_success(uart, "Ready to accept commands!");
    print_help(uart);

    char line[80];
    while (true) {
        Ui::prompt_user(uart, line, sizeof(line));

        std::optional<Command> cmd_opt = Command::from_string(line);
        if (!cmd_opt.has_value()) {
            log_error(uart, "Unrecognized command.");
            continue;
        }

        const Command& cmd = cmd_opt.value();

        switch (cmd.kind) {
            case Command::ADDTR: {
                const uint8_t train = (uint8_t)cmd.addtr.no;
                if (!is_valid_train(train)) {
                    log_error(uart, "Invalid train id.");
                    continue;
                }

                log_line(
                    uart, VT_CYAN
                    "Place the train somewhere on the track." VT_NOFMT ENDL
                    "It's okay if the train is running, we'll send a stop "
                    "command." ENDL VT_GREEN
                    "Press [ENTER] once the train is on the track." VT_NOFMT);
                wait_for_enter(uart);

                // register the train with the oracle
                track_oracle.calibrate_train(train);

                // log_line(uart,
                //          VT_GREEN "Press [ENTER] to start the train"
                //          VT_NOFMT);
                // wait_for_enter(uart);

                // track_oracle.set_train_speed(train, 14);
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
                log_error(uart, "Invalid command.");
            } break;
            case Command::LIGHT: {
                // IMPROVEMENT: actually implement light command
                log_error(uart, "Invalid command.");
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
                log_success(uart, "Manually reversed train %u", cmd.rv.no);
            } break;
            case Command::STOP: {
                // IMPROVEMENT: actually implement stop command
                log_error(uart, "Invalid command.");
            } break;
            case Command::SW: {
                track_oracle.set_branch_dir((uint8_t)cmd.sw.no, cmd.sw.dir);
                log_success(uart, "Manually set branch %u to %s", cmd.sw.no,
                            cmd.sw.dir == Marklin::BranchDir::Straight
                                ? "straight"
                                : "curved");
            } break;
            case Command::TR: {
                track_oracle.set_train_speed((uint8_t)cmd.tr.no,
                                             (uint8_t)cmd.tr.speed);
                log_success(uart, "Manually set train %u to speed %u",
                            cmd.tr.no, cmd.tr.speed);
            } break;
            case Command::PATH: {
                size_t distance = 0;
                const track_node* path[TRACK_MAX];
                int path_len = track_graph.shortest_path(
                    cmd.path.source, cmd.path.dest, path, TRACK_MAX, distance);
                if (path_len < 0) {
                    log_error(uart, "No route from %c%u to %c%u",
                              cmd.path.source.group, cmd.path.source.idx,
                              cmd.path.dest.group, cmd.path.dest.idx);
                    break;
                } else {
                    char line[1024] = {'\0'};
                    size_t n = snprintf(line, sizeof(line),
                                        "Path found (len=%d, dist=%u):",
                                        path_len, distance);
                    for (int i = 0; i < path_len; i++) {
                        assert(path[i] != nullptr);
                        n += snprintf(line + n, sizeof(line) - n, " %s",
                                      path[i]->name);
                    }
                    log_success(uart, "%s", line);
                }
            } break;
            default:
                panic("somehow parsed an invalid command!");
        }
    }
}

static void t1_main(int clock, int uart, const TermSize& term_size) {
    (void)clock;

    Ui::render_initial_screen(uart, term_size);

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

    // spawn the perf task
    {
        int tid = Create(0, SysPerf::Task);
        SysPerf::TaskCfg cfg = {.term_size = term_size};
        Send(tid, (char*)&cfg, sizeof(cfg), nullptr, 0);
    }

    t1_main(clock, uart, term_size);
}
