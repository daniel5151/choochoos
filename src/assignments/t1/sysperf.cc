#include "sysperf.h"

#include <cstdio>
#include <cstring>

#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

static const char BLOCK_CHARS[6][4] = {"▁", "▂", "▃", "▅", "▆", "▇"};

void SysPerf::Task() {
    int tid;
    TaskCfg cfg;
    int n = Receive(&tid, (char*)&cfg, sizeof(cfg));
    assert(n == sizeof(cfg));
    Reply(tid, nullptr, 0);

    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);

    assert(clock >= 0);
    assert(uart >= 0);

    uint8_t past_perf[cfg.term_size.width - 16];
    memset(past_perf, 0, sizeof(past_perf));

    char outbuf[cfg.term_size.width * 3];
    memset(outbuf, 0, sizeof(outbuf));

    perf_t perf;
    for (;;) {
        Perf(&perf);

        memmove(past_perf, past_perf + 1, sizeof(past_perf) - 1);
        past_perf[sizeof(past_perf) - 1] =
            (uint8_t)((100 - perf.idle_time_pct) / 18);

        size_t i = 0;
        for (uint8_t p : past_perf) {
            i += sprintf(outbuf + i, "%s", BLOCK_CHARS[p]);
        }

        Uart::Printf(uart, COM2, VT_SAVE VT_TOPLEFT VT_HIDECUR
                     "CPU Usage (%02lu%%) %s" VT_SHOWCUR VT_RESTORE,
                     (100 - perf.idle_time_pct), outbuf);

        Clock::Delay(clock, (int)100);
    }
}
