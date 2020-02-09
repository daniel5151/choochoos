#include <climits>

#include "common/bwio.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/nameserver.h"

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
    int myparent = MyParentTid();
    int n = Send(myparent, nullptr, 0, (char*)&cfg, sizeof(cfg));
    assert(n == sizeof(cfg));

    debug("DelayerTask starting: tid=%d delay_interval=%u num_delays=%u" ENDL,
          mytid, cfg.delay, cfg.n);
    for (size_t i = 0; i < cfg.n; i++) {
        Clock::Delay(clockserver, (int)cfg.delay);
        bwprintf(COM2, "time=%-3d tid=%d interval=%-2u completed=%2u/%u" ENDL,
                 Clock::Time(clockserver), mytid, cfg.delay, i + 1, cfg.n);
    }
    Send(myparent, nullptr, 0, nullptr, 0);
}

static Config configs[4] = {{3, 10, 20}, {4, 23, 9}, {5, 33, 6}, {6, 71, 3}};

void FirstUserTask() {
    Create(1, NameServer::Task);
    int clockserver = Create(INT_MAX, Clock::Server);

    for (auto& cfg : configs) {
        int tid = Create(10 - cfg.priority, DelayerTask);
        if (tid < 0) panic("could not spawn DelayerTask");
    }

    int tid;
    for (auto& cfg : configs) {
        Receive(&tid, nullptr, 0);
        Reply(tid, (char*)&cfg, sizeof(cfg));
    }
    for (auto& _ : configs) {
        (void)_;
        Receive(&tid, nullptr, 0);
        Reply(tid, nullptr, 0);
    }
    Clock::Shutdown(clockserver);
    NameServer::Shutdown();
}