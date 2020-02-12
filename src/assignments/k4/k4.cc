#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include <climits>

void FirstUserTask() {
    int clock = Create(INT_MAX, Clock::Server);
    int uart = Create(INT_MAX, Uart::Server);
    Uart::Printf(uart, COM2, "k4" ENDL);
    Uart::Shutdown(uart);
    Clock::Shutdown(clock);
}
