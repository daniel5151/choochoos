#include <cctype>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "marklin.h"
#include "sysperf.h"
#include "track_oracle.h"
#include "ui.h"

static inline Marklin::Track query_user_for_track(int uart) {
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

static inline bool is_valid_train(uint8_t no) {
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

static inline uint8_t query_user_for_train(int uart) {
    uint8_t train_id;
    while (true) {
        log_line(uart, VT_GREEN "Enter the train you'll be using" VT_NOFMT);
        char buf[4];
        Ui::prompt_user(uart, buf, sizeof(buf));

        int t = 0;
        int matches = sscanf(buf, "%d", &t);
        train_id = (uint8_t)t;

        if (matches == 1) {
            bool valid_train = false;
            for (uint8_t valid_id : Marklin::VALID_TRAINS) {
                if (train_id == valid_id) {
                    valid_train = true;
                }
            }
            if (valid_train) {
                break;
            }
        }

        log_line(uart, VT_RED "Invalid train number." VT_NOFMT);
    }
    return train_id;
}

static inline void wait_for_enter(int uart) {
    char dummy;
    Ui::prompt_user(uart, &dummy, sizeof(dummy));
}

static inline bool parse_route_cmd(char* line,
                                   uint8_t& train,
                                   Marklin::sensor_t& sensor,
                                   int& offset) {
    char* token;

    size_t train_u;
    char sensor_group;
    size_t sensor_idx;

    // "route"
    token = strtok(line, " ");
    if (!token || strcmp(token, "route") != 0) return false;

    // "<tr>"
    token = strtok(nullptr, " ");
    if (!token || sscanf(token, "%u", &train_u) != 1) return false;

    train = (uint8_t)train_u;

    // "<sensor>"
    token = strtok(nullptr, " ");
    if (!token || sscanf(token, "%c%u", &sensor_group, &sensor_idx) != 2)
        return false;

    sensor.group = (char)toupper(sensor_group);
    sensor.idx = (uint8_t)sensor_idx;

    // "<offset>"
    token = strtok(nullptr, " ");
    if (!token || sscanf(token, "%d", &offset) != 1) return false;

    return true;
}

static int stopping_distance(uint8_t train, uint8_t speed) {
    // TODO use our calibration data
    (void)train;
    (void)speed;
    return 250;  // 25 cm
}

static int stopping_time(uint8_t train, uint8_t speed) {
    // TODO use our calibration data
    (void)train;
    (void)speed;
    return 300;  // 3 seconds
}

static void CmdTask() {
    int uart = WhoIs(Uart::SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);
    assert(uart >= 0);
    assert(clock >= 0);

    // This looks up the oracle task in the nameserver.
    TrackOracle track_oracle = TrackOracle();

    log_line(uart, VT_YELLOW "Ready to accept commands!" VT_NOFMT);

    char line[80];
    while (true) {
        Ui::prompt_user(uart, line, sizeof(line));

        if (strcmp(line, "q") == 0) {
            Uart::Putstr(uart, COM2, VT_RESET);
            Uart::Flush(uart, COM2);
            Shutdown();
        }

        // otherwise, only other command is `route <tr> <sensor> <offset>`
        uint8_t train;
        Marklin::sensor_t sensor;
        int offset;

        if (!parse_route_cmd(line, train, sensor, offset)) {
            log_line(uart, VT_RED "Invalid command." VT_NOFMT);
            continue;
        }

        if (!is_valid_train(train)) {
            log_line(uart, VT_RED "Invalid train id." VT_NOFMT);
            continue;
        }

        if (!is_valid_sensor(sensor)) {
            log_line(uart, VT_RED "Invalid sensor." VT_NOFMT);
            continue;
        }

        log_line(uart,
                 VT_CYAN "Routing train %u to sensor %c%u + offset %d" VT_NOFMT,
                 train, sensor.group, sensor.idx, offset);

        track_oracle.set_train_speed(train, 8);
        int stop_at_offset = offset - stopping_distance(train, 8);
        Marklin::track_pos_t send_stop_at_pos = {.sensor = sensor,
                                                 .offset_mm = stop_at_offset};
        log_line(uart, "Waiting for train %u to reach sensor %c%u%c%dmm ...",
                 train, sensor.group, sensor.idx,
                 stop_at_offset < 0 ? '-' : '+', std::abs(stop_at_offset));
        track_oracle.wake_at_pos(train, send_stop_at_pos);
        log_line(uart,
                 "Sending speed=0 to train %u. Waiting for train to stop...",
                 train);
        track_oracle.set_train_speed(train, 0);
        Clock::Delay(clock, stopping_time(train, 8));
        log_line(uart, "Stopped! (hopefully)");
    }
}

static void t1_main(int clock, int uart) {
    (void)clock;

    // determine which track to use
    Marklin::Track track_id = query_user_for_track(uart);

    // create the track oracle (which also instantiates the track)
    TrackOracle track_oracle = TrackOracle(track_id);

    // determine which train to use
    uint8_t train_id = query_user_for_train(uart);

    log_line(
        uart, VT_YELLOW
        "Place the train somewhere on the track." VT_NOFMT ENDL
        "It's okay if the train is running, we'll send a stop command." ENDL
            VT_GREEN "Press [ENTER] once the train is on the track.");
    wait_for_enter(uart);

    // register the train with the oracle
    track_oracle.calibrate_train(train_id);

    // TODO: parse commands, including a new command to route to a specific
    // position
    log_line(uart, VT_GREEN "Press [ENTER] to start the train");
    wait_for_enter(uart);

    Create(0, CmdTask);

    track_oracle.set_train_speed(train_id, 14);
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
