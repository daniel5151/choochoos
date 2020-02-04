#include <climits>
#include "user/dbg.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"

void Idle() {
    while (true) {
        char c = (char)bwgetc(COM2);
        if (c == 'q') return;
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
    Clock::Delay(clockserver, 1);
    printf("got time %d" ENDL, Clock::Time(clockserver));
    Clock::Delay(clockserver, 0);
    printf("got time %d" ENDL, Clock::Time(clockserver));
}
