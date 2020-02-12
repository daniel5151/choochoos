#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include <climits>

void FirstUserTask() {
    int clockserver = Create(INT_MAX, Clock::Server);

    bwprintf(COM2, "k4" ENDL);
    Clock::Shutdown(clockserver);
}
