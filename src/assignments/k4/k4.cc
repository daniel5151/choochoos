#include <climits>
#include <cstdlib>
#include <cstring>

#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

void TimerTask() {
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);

    assert(clock >= 0);
    assert(uart >= 0);

    while (true) {
        int ticks = Clock::Time(clock);
        if (ticks < 0) return;

        Uart::Printf(uart, COM2,
                     VT_SAVE VT_ROWCOL(2, 1) "%d:%02d:%d" VT_RESTORE,
                     ticks / (100 * 60), (ticks / 100) % 60, (ticks / 10) % 10);
        Clock::Delay(clock, 10);
    }
}

void LoggerTask() {
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);
    int entry = 0;
    const int start_row = 3;
    const int lines = 10;

    assert(clock >= 0);
    assert(uart >= 0);

    srand(0);

    while (true) {
        for (int i = 0; i < lines; i++) {
            int row = start_row + i;
            entry++;
            Uart::Printf(
                uart, COM2,
                VT_SAVE VT_ROWCOL_FMT
                "time=%d log entry %d %08x%08x%08x%08x" ENDL VT_RESTORE,
                row, 1, Clock::Time(clock), entry, rand(), rand(), rand(),
                rand());
            Clock::Delay(clock, 10);
        }
    }
}

void InputTask() {
    int uart = WhoIs(Uart::SERVER_ID);

    assert(uart >= 0);

    char line[1024];
    while (true) {
        Uart::Putstr(uart, COM2, "> ");
        Uart::Getline(uart, COM2, line, sizeof(line));
        Uart::Printf(uart, COM2, VT_CLEARLN "you wrote '%s'" ENDL, line);

        // echo whatever you wrote to the track
        // TODO parse the input, write out the right track command
        Uart::Printf(uart, COM1, "%s", line);
    }
}

void PrintTask() {
    int uart = WhoIs(Uart::SERVER_ID);
    assert(uart >= 0);
    Uart::Printf(
        uart, COM2,
        "tid=%d "
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
        "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
        "aliquip ex ea commodo consequat. Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
        "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
        "culpa qui officia deserunt mollit anim id est laborum." ENDL,
        MyTid());

    Send(MyParentTid(), nullptr, 0, nullptr, 0);
}

void FirstUserTask() {
    int clock = Create(1000, Clock::Server);
    int uart = Create(1000, Uart::Server);

    assert(clock >= 0);
    assert(uart >= 0);

    Uart::Putstr(uart, COM2, VT_CLEAR VT_SET_SCROLL(14, 20) VT_ROWCOL(20, 1));
    Create(0, TimerTask);
    Create(0, LoggerTask);

    Uart::Printf(
        uart, COM2,
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
        "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
        "aliquip ex ea commodo consequat. Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
        "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
        "culpa qui officia deserunt mollit anim id est laborum." ENDL);

    Create(0, InputTask);
}
