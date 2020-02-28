#include <cstdio>
#include <initializer_list>

#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "marklin.h"
#include "sysperf.h"

static void init_track(int uart, Marklin::Controller marklin) {
    Uart::Printf(uart, COM2, "Initializing Track..." ENDL);

    // ensure the track is on
    marklin.send_go();

    Uart::Printf(uart, COM2, "Stopping all trains..." ENDL);
    auto train = Marklin::TrainState(0);
    train.set_speed(0);
    train.set_light(false);
    for (uint8_t id : Marklin::VALID_TRAINS) {
        train.set_id(id);
        marklin.update_train(train);
        marklin.flush();
    }

    // set all the branches to curved
    Marklin::BranchState branches[sizeof(Marklin::VALID_SWITCHES)];
    for (size_t i = 0; auto& b : branches) {
        const uint8_t id = Marklin::VALID_SWITCHES[i++];

        b.set_id(id);
        b.set_dir(Marklin::BranchState::Curved);

        // ...but make outer-ring branches straight
        for (size_t except_id : {6, 7, 8, 9, 14, 15}) {
            if (id == except_id) {
                b.set_dir(Marklin::BranchState::Straight);
                break;
            }
        }
    }

    Uart::Printf(uart, COM2, "Setting switch positions..." ENDL);
    marklin.update_branches(branches, sizeof(Marklin::VALID_SWITCHES));
    marklin.flush();

    Uart::Printf(uart, COM2, "Track has been initialized!" ENDL);
}

static void t1_main(int clock, int uart) {
    auto marklin = Marklin::Controller(uart);
    (void)clock;

    // reset the track
    init_track(uart, marklin);

    // determine which train to use
    uint8_t train_id;
    while (true) {
        Uart::Printf(uart, COM2, "Please Enter the train you'll be using: ");
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

    Uart::Printf(uart, COM2,
                 "Press [ENTER] after ensuring that the train is somewhere on "
                 "the outer loop." ENDL);
    {
        char dummy;
        Uart::Getline(uart, COM2, &dummy, sizeof(dummy));
    }

    Uart::Printf(uart, COM2, "Calibrating the track..." ENDL);

    // let 'er rip at max speed
    marklin.update_train(Marklin::TrainState(train_id, 14));

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
