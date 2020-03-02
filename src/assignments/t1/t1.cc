#include <cctype>
#include <cstdio>
#include <initializer_list>

#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "marklin.h"
#include "sysperf.h"
#include "track_oracle.h"

static inline Marklin::Track query_user_for_track(int uart) {
    while (true) {
        Uart::Printf(uart, COM2, "Enter the track you'll be using (A or B): ");
        char buf[2];
        Uart::Getline(uart, COM2, buf, sizeof(buf));

        switch (tolower(buf[0])) {
            case 'a':
                return Marklin::Track::A;
            case 'b':
                return Marklin::Track::B;
            default:
                Uart::Printf(uart, COM2, "Invalid track value." ENDL);
        }
    }
}

static inline uint8_t query_user_for_train(int uart) {
    uint8_t train_id;
    while (true) {
        Uart::Printf(uart, COM2, "Enter the train you'll be using: ");
        char buf[4];
        Uart::Getline(uart, COM2, buf, sizeof(buf));

        int matches = sscanf(buf, "%hhu", &train_id);
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

        Uart::Printf(uart, COM2, "Invalid train number." ENDL);
    }
    return train_id;
}

static inline void wait_for_enter(int uart) {
    char dummy;
    Uart::Getline(uart, COM2, &dummy, sizeof(dummy));
}

static void t1_main(int clock, int uart) {
    (void)clock;

    // determine which track to use
    Marklin::Track track_id = query_user_for_track(uart);

    // create the track oracle (which also instantiates the track)
    TrackOracle track_oracle = TrackOracle(track_id);

    // determine which train to use
    uint8_t train_id = query_user_for_train(uart);

    Uart::Printf(uart, COM2,
                 "Press [ENTER] after placing the train somewhere on the train "
                 "set. It's okay if the train is running, we'll send a stop "
                 "command." ENDL);
    wait_for_enter(uart);

    // register the train with the oracle
    track_oracle.calibrate_train(train_id);

    // TODO: enter the main application loop
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

    t1_main(clock, uart);
}
