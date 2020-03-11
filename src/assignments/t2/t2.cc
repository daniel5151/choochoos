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
#include "track_graph.h"
#include "track_oracle.h"
#include "ui.h"

static inline void print_help(const Ui& ui) {
    // clang-format off
    log_line(ui,
        "t1 commands:" ENDL
        "  addtr <train>                   - register a train with the track" ENDL
        "  route <train> <sensor> <offset> - route train to given sensor + offset" ENDL
        "  tr <train> <speed>              - set a train to a certain speed" ENDL
        "  mkloop                          - reset track to have a loop" ENDL
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

static inline void wait_for_enter(const Ui& ui) {
    char dummy;
    ui.prompt_user(&dummy, sizeof(dummy));
}

static void do_route_cmd(const Ui& ui,
                         const int clock,
                         TrackOracle& track_oracle,
                         const TrackGraph& track_graph,
                         const Command::route_t& cmd) {
    Marklin::sensor_t sensor = {.group = cmd.sensor_group,
                                .idx = (uint8_t)cmd.sensor_idx};
    const uint8_t train = (uint8_t)cmd.train;

    if (!is_valid_train(train)) {
        log_error(ui, "Invalid train id.");
        return;
    }

    if (!is_valid_sensor(sensor)) {
        log_error(ui, "Invalid sensor.");
        return;
    }

    auto td_opt = track_oracle.query_train(train);
    if (!td_opt.has_value()) {
        log_error(ui,
                  "train %u is not calibrated. Please run 'addtr %u' first.",
                  train, train);
        return;
    }
    const train_descriptor_t& td = td_opt.value();

    log_success(ui, "Routing train %u (%c%u) to sensor %c%u + %dmm", train,
                td.pos.sensor.group, td.pos.sensor.idx, sensor.group,
                sensor.idx, cmd.offset);

    // find the shortest path
    constexpr size_t MAX_PATH_LEN = 64;  // 64 is overkill, but it's fine
    const track_node* path[MAX_PATH_LEN];
    size_t total_path_distance = 0;
    int path_len = 0;

    path_len = track_graph.shortest_path(td.pos.sensor, sensor, path,
                                         MAX_PATH_LEN, total_path_distance);
    if (path_len < 0) {
        // that's okay, all this means is that we might be stopped at a terminal
        // of the track. if our speed is zero, then we could try to reverse, and
        // then route.
        if (td.speed != 0) {
            log_error(ui,
                      "could not find path, even though the train was running "
                      "(i.e: it's assumed to be in a loop)");
            return;
        }

        track_oracle.reverse_train(train);

        td_opt = track_oracle.query_train(train);
        assert(td_opt.has_value());
        const train_descriptor_t& td = td_opt.value();

        path_len = track_graph.shortest_path(td.pos.sensor, sensor, path,
                                             MAX_PATH_LEN, total_path_distance);

        if (path_len < 0) {
            log_error(ui,
                      "could not find path, even after reversing the train!");
            return;
        }
    }

    // FIXME: handle zero-len paths
    if (path_len == 0) {
        log_error(ui, "zero len paths not supported :/");
        return;
    }

    // FIXME: instead of doing "JIT" branch updates, we just set all of them,
    // and then wait for the train to hit certain checkpoints.

    for (size_t i = 0; i < (size_t)path_len; i++) {
        assert(path[i] != nullptr);
        const track_node& n = *path[i];
        log_line(ui, "%s", n.name);

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
                    log_line(ui, "set branch %d to straight", n.num);
                    dir = Marklin::BranchDir::Straight;
                } else if (n.edge[DIR_CURVED].dest == &next_n) {
                    log_line(ui, "set branch %d to curved", n.num);
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
        log_warning(ui, "Found --dry-run flag, not actually routing...");
        return;
    }

    int stopping_distance = Calibration::stopping_distance(train, 8);
    int slowdown_distance = 2000;

    // send the stop command (speed=0) `stopping_distance`mm before the target
    // sensor, or `total_path_distance` - `stopping_distance` _after_ the
    // starting position.
    Marklin::track_pos_t send_stop_at_pos = td.pos;
    send_stop_at_pos.offset_mm +=
        ((int)total_path_distance - stopping_distance);
    send_stop_at_pos = track_oracle.normalize(send_stop_at_pos);

    // if the path is long enough, set the speed to 14 initially, and then set
    // the speed to 8 `slowdown_distance`mm before we send the stop command.
    Marklin::track_pos_t send_slow_at_pos = td.pos;
    send_slow_at_pos.offset_mm +=
        ((int)total_path_distance - stopping_distance - slowdown_distance);
    send_slow_at_pos = track_oracle.normalize(send_slow_at_pos);

    ///////////////////////////////

    if ((int)total_path_distance > slowdown_distance + stopping_distance) {
        log_success(ui,
                    "Set speed to 14. Waiting for train %u to reach sensor "
                    "%c%u %c %dmm ...",
                    train, send_slow_at_pos.sensor.group,
                    send_slow_at_pos.sensor.idx,
                    send_slow_at_pos.offset_mm < 0 ? '-' : '+',
                    std::abs(send_slow_at_pos.offset_mm));
        // let 'er eat at 14
        track_oracle.set_train_speed(train, 14);

        bool ok = track_oracle.wake_at_pos(train, send_slow_at_pos);
        if (!ok) {
            log_error(ui, "wake_at_pos for slow-down failed unexpectedly!");
            track_oracle.set_train_speed(train, 0);
            return;
        }
    }

    track_oracle.set_train_speed(train, 8);

    log_success(
        ui,
        "Set speed to 8. Waiting for train %u to reach sensor %c%u %c %dmm ...",
        train, send_stop_at_pos.sensor.group, send_stop_at_pos.sensor.idx,
        send_stop_at_pos.offset_mm < 0 ? '-' : '+',
        std::abs(send_stop_at_pos.offset_mm));

    bool ok = track_oracle.wake_at_pos(train, send_stop_at_pos);

    if (!ok) {
        log_error(ui, "wake_at_pos for stop failed unexpectedly!");
        track_oracle.set_train_speed(train, 0);
        return;
    }

    log_success(ui,
                "Sending speed=0 to train %u. Waiting for train to "
                "stop...",
                train);
    track_oracle.set_train_speed(train, 0);

    Clock::Delay(clock, Calibration::stopping_time(train, 8));
    log_success(ui, "Stopped!");
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
    int clock = WhoIs(Clock::SERVER_ID);
    assert(clock >= 0);

    const Ui ui = Ui();

    // This looks up the oracle task in the nameserver.
    TrackOracle track_oracle = TrackOracle();

    // We keep a copy track of the track graph ourselves so we can query it when
    // routing.
    TrackGraph track_graph(cfg.track);

    log_success(ui, "Ready to accept commands!");
    print_help(ui);

    char line[80];
    while (true) {
        ui.prompt_user(line, sizeof(line));

        std::optional<Command> cmd_opt = Command::from_string(line);
        if (!cmd_opt.has_value()) {
            log_error(ui, "Unrecognized command.");
            continue;
        }

        const Command& cmd = cmd_opt.value();

        switch (cmd.kind) {
            case Command::ADDTR: {
                const uint8_t train = (uint8_t)cmd.addtr.no;
                if (!is_valid_train(train)) {
                    log_error(ui, "Invalid train id.");
                    continue;
                }

                log_line(
                    ui, VT_CYAN
                    "Place the train somewhere on the track." VT_NOFMT ENDL
                    "It's okay if the train is running, we'll send a stop "
                    "command." ENDL VT_GREEN
                    "Press [ENTER] once the train is on the track." VT_NOFMT);
                wait_for_enter(ui);

                // register the train with the oracle
                track_oracle.calibrate_train(train);

                // log_line(ui,
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
                log_line(ui, "Position %c%u@%d normalizes to %c%u@%d.",
                         pos.sensor.group, pos.sensor.idx, pos.offset_mm,
                         npos.sensor.group, npos.sensor.idx, npos.offset_mm);
            } break;
            case Command::HELP: {
                print_help(ui);
            } break;
            case Command::GO: {
                // IMPROVEMENT: actually implement go command
                log_error(ui, "Invalid command.");
            } break;
            case Command::LIGHT: {
                // IMPROVEMENT: actually implement light command
                log_error(ui, "Invalid command.");
            } break;
            case Command::Q: {
                ui.shutdown();
                Shutdown();
            } break;
            case Command::ROUTE: {
                do_route_cmd(ui, clock, track_oracle, track_graph, cmd.route);
            } break;
            case Command::RV: {
                track_oracle.reverse_train((uint8_t)cmd.rv.no);
                log_success(ui, "Manually reversed train %u", cmd.rv.no);
            } break;
            case Command::STOP: {
                // IMPROVEMENT: actually implement stop command
                log_error(ui, "Invalid command.");
            } break;
            case Command::SW: {
                track_oracle.set_branch_dir((uint8_t)cmd.sw.no, cmd.sw.dir);
                log_success(ui, "Manually set branch %u to %s", cmd.sw.no,
                            cmd.sw.dir == Marklin::BranchDir::Straight
                                ? "straight"
                                : "curved");
            } break;
            case Command::TR: {
                track_oracle.set_train_speed((uint8_t)cmd.tr.no,
                                             (uint8_t)cmd.tr.speed);
                log_success(ui, "Manually set train %u to speed %u", cmd.tr.no,
                            cmd.tr.speed);
            } break;
            case Command::PATH: {
                size_t distance = 0;
                const track_node* path[TRACK_MAX];
                int path_len = track_graph.shortest_path(
                    cmd.path.source, cmd.path.dest, path, TRACK_MAX, distance);
                if (path_len < 0) {
                    log_error(ui, "No route from %c%u to %c%u",
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
                    log_success(ui, "%s", line);
                }
            } break;
            case Command::MKLOOP: {
                track_oracle.make_loop();
                log_success(ui, "Created a loop!");
            } break;
            default:
                panic("somehow parsed an invalid command!");
        }
    }
}

void FirstUserTask() {
    int clock = Create(1000, Clock::Server);
    int uart = Create(1000, Uart::Server);

    assert(clock >= 0);
    assert(uart >= 0);

    const Ui ui = Ui();

    ui.render_initial_screen();

    // determine which track to use
    std::optional<Marklin::Track> track_id_opt;
    while (!track_id_opt.has_value()) {
        log_useraction(ui, "Enter the track you'll be using (A or B)");
        char buf[2];
        ui.prompt_user(buf, sizeof(buf));

        switch (tolower(buf[0])) {
            case 'a':
                track_id_opt = Marklin::Track::A;
                break;
            case 'b':
                track_id_opt = Marklin::Track::B;
                break;
            default:
                log_error(ui, "Invalid track value.");
        }
    }
    const Marklin::Track track_id = track_id_opt.value();

    // create the track oracle (which also initializes the track)
    TrackOracle track_oracle = TrackOracle(track_id);
    (void)track_oracle;

    // CmdTask has higher priority than the track oracle's sensor query loop so
    // that the oracle commands it sends trump sensor queries.
    int cmdtask = Create(1, CmdTask);
    {
        CmdTaskCfg cfg = {.track = track_id};
        int n = Send(cmdtask, (char*)&cfg, sizeof(cfg), nullptr, 0);
        assert(n == 0);
    }
}
