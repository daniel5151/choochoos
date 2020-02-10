#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/nameserver.h"

#include <climits>

void Task() {
    int my_tid = MyTid();
    int clockserver = NameServer::WhoIs(Clock::SERVER_ID);
    Clock::Delay(clockserver, 50);
    bwprintf(COM2, "tid=%d %d" ENDL, my_tid, Clock::Time(clockserver));
    Clock::DelayUntil(clockserver, 150);
    bwprintf(COM2, "tid=%d %d" ENDL, my_tid, Clock::Time(clockserver));

    Send(MyParentTid(), nullptr, 0, nullptr, 0);
}

void FirstUserTask() {
    int clockserver = Create(INT_MAX, Clock::Server);

    int delays[] = {0, 20, 10, 10};
    for (int delay : delays) {
        Clock::Delay(clockserver, delay);
        Create(5, Task);
    }

    int tid;
    for (auto _ : delays) {
        (void)_;
        Receive(&tid, nullptr, 0);
        Reply(tid, nullptr, 0);
    }

    Clock::Shutdown(clockserver);
}
