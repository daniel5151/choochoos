#include <climits>

#include "common/bwio.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/nameserver.h"

// TODO james: this is DEFINITELY NOT where this function belongs.
// We should either put this in src/user/tasks/idle.cc, or the kernel.
void Idle() {
    while (true) {
        char c = (char)bwgetc(COM2);
        if (c == 'q') return;
    }
}

struct Config {
    // smaller priority is higher
    int priority;
    size_t delay;
    size_t n;
};

void DelayerTask() {
    Config cfg;
    int clockserver = NameServer::WhoIs(Clock::SERVER_ID);
    assert(clockserver >= 0);
    int mytid = MyTid();
    int n = Send(MyParentTid(), nullptr, 0, (char*)&cfg, sizeof(cfg));
    assert(n == sizeof(cfg));

    bwprintf(
        COM2,
        "DelayerTask starting: tid=%d delay_interval=%u num_delays=%u" ENDL,
        mytid, cfg.delay, cfg.n);
    for (size_t i = 0; i < cfg.n; i++) {
        Clock::Delay(clockserver, (int)cfg.delay);
        bwprintf(COM2,
                 "time=%-3d tid=%-2d delay_interval=%-2u completed=%-2u" ENDL,
                 Clock::Time(clockserver), mytid, cfg.delay, i + 1);
    }
}

static Config configs[4] = {{3, 10, 20}, {4, 23, 9}, {5, 33, 6}, {6, 71, 3}};

void FirstUserTask() {
    Create(1, NameServer::Task);
    Create(0, Idle);
    int clockserver = Create(INT_MAX, Clock::Server);
    (void)clockserver;

    for (auto& cfg : configs) {
        int tid = Create(10 - cfg.priority, DelayerTask);
        if (tid < 0) panic("could not spawn DelayerTask");
    }

    int tid;
    for (auto& cfg : configs) {
        Receive(&tid, nullptr, 0);
        Reply(tid, (char*)&cfg, sizeof(cfg));
    }
}
