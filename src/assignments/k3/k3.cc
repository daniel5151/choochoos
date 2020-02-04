#include <climits>
#include "user/dbg.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"

void Idle() {
    while (true) {
        // Yield();
    }
}
void FirstUserTask() {
    Create(0, Idle);
    int clockserver = Create(INT_MAX, Clock::Server);
    printf("started clock server" ENDL);

    printf("got time %d" ENDL, Clock::Time(clockserver));

    Clock::Delay(clockserver, 100);
    printf("got time %d" ENDL, Clock::Time(clockserver));
    Clock::Delay(clockserver, 50);
    printf("got time %d" ENDL, Clock::Time(clockserver));
}
