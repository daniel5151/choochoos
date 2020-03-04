#include <cctype>
#include <cstdio>
#include <initializer_list>

#include <cstring>  // strcmp

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

static void CmdTask() {
    int uart = WhoIs(Uart::SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);
    assert(uart >= 0);
    assert(clock >= 0);

    log_line(uart, VT_YELLOW "Ready to accept commands!" VT_NOFMT);

    char line[80];
    while (true) {
        Ui::prompt_user(uart, line, sizeof(line));

        // TODO parse and handle the command (this can block)
        if (strcmp(line, "q") == 0) {
            Uart::Putstr(uart, COM2, VT_RESET);
            Uart::Flush(uart, COM2);
            Shutdown();
        }

        log_line(uart, "you wrote '%s'", line);
        Clock::Delay(clock, 100);
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
            VT_GREEN "Press [ENTER] once the train is on the track."
        );
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
