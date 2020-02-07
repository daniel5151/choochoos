#include <climits>

#include "common/bwio.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"

// TODO james: this is DEFINITELY NOT where this function belongs.
// We should either put this in src/user/tasks/idle.cc, or the kernel.
void Idle() {
    while (true) {
        char c = (char)bwgetc(COM2);
        if (c == 'q') return;
    }
}

void FirstUserTask() {
    Create(0, Idle);
    int clockserver = Create(INT_MAX, Clock::Server);
    bwprintf(COM2, "started clock server" ENDL);

    bwprintf(COM2, "got time %d" ENDL, Clock::Time(clockserver));

    Clock::Delay(clockserver, 100);
    bwprintf(COM2, "got time %d" ENDL, Clock::Time(clockserver));
    Clock::Delay(clockserver, 50);
    bwprintf(COM2, "got time %d" ENDL, Clock::Time(clockserver));
    Clock::Delay(clockserver, 1);
    bwprintf(COM2, "got time %d" ENDL, Clock::Time(clockserver));
    Clock::Delay(clockserver, 0);
    bwprintf(COM2, "got time %d" ENDL, Clock::Time(clockserver));
}
