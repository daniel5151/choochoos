#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include <climits>

void TimerTask() {
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);

    assert(clock >= 0);
    assert(uart >= 0);

    while (true) {
        Clock::Delay(clock, 10);
        int ticks = Clock::Time(clock);
        Uart::Printf(uart, COM2,
                     VT_SAVE VT_ROWCOL(2, 60) "%d:%02d:%d" VT_RESTORE,
                     ticks / (100 * 60), (ticks / 100) % 60, (ticks / 10) % 10);
    }
}

void FirstUserTask() {
    int clock = Create(INT_MAX, Clock::Server);
    int uart = Create(INT_MAX, Uart::Server);

    Create(10, TimerTask);

    Uart::Printf(uart, COM2, VT_CLEAR);
    while (true) {
        Uart::Printf(uart, COM2, "k4" ENDL);
        Clock::Delay(clock, 150);
    }
}
