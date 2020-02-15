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
                     VT_SAVE VT_ROWCOL(2, 60) "%d:%02d:%d" VT_RESTORE,
                     ticks / (100 * 60), (ticks / 100) % 60, (ticks / 10) % 10);
        Clock::Delay(clock, 10);
    }
}

void LoggerTask() {
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);

    assert(clock >= 0);
    assert(uart >= 0);

    for (int i = 0;; i++) {
        Uart::Printf(uart, COM2, "time=%d log entry %d %08x%08x%08x%08x" ENDL,
                     Clock::Time(clock), i, rand(), rand(), rand(), rand());
        Clock::Delay(clock, 49);
    }
}

void InputTask() {
    int uart = WhoIs(Uart::SERVER_ID);

    assert(uart >= 0);

    char line[24];
    while (true) {
        Uart::Putstr(uart, COM2, "> ");
        Uart::Getline(uart, COM2, line, sizeof(line));
        Uart::Printf(uart, COM2, VT_CLEARLN "you wrote '%s'" ENDL, line);
    }
}

void PrintTask() {
    int uart = WhoIs(Uart::SERVER_ID);
    assert(uart >= 0);
    Uart::Printf(uart, COM2,
                 "tid=%d 0123456789012345678901234567890123456789" ENDL,
                 MyTid());
}

void FirstUserTask() {
    int clock = Create(1000, Clock::Server);
    int uart = Create(1000, Uart::Server);

    assert(clock >= 0);
    assert(uart >= 0);

    // Create(10, InputTask);
    // Create(0, PrintTask);
    // Create(1, PrintTask);
    // Uart::Putstr(uart, COM2, "0123456789012345678901234567890123456789"
    // ENDL); Create(0, PrintTask);

    // Create(0, TimerTask);
    // Create(0, LoggerTask);
    Uart::Putstr(
        uart, COM2,
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
        "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
        "aliquip ex ea commodo consequat. Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
        "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
        "culpa qui officia deserunt mollit anim id est laborum." ENDL);
    Clock::Shutdown(clock);
}
