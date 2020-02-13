#include <climits>

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

    Create(0, TimerTask);

    Uart::Putstr(uart, COM2, VT_CLEAR VT_SET_SCROLL(4, 20) VT_ROWCOL(20, 1));
    Uart::Putc(uart, COM2, 'x');

    for (int i = 1;; i++) {
        Uart::Printf(uart, COM2, "%d: k4" ENDL, i);
        Clock::Delay(clock, 25);
    }
}
