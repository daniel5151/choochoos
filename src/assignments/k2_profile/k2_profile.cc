#include <cstring>
#include <initializer_list>

#include "common/bwio.h"
#include "common/ts7200.h"

#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/nameserver.h"

#define TIMER3_LDR (volatile uint32_t*)(TIMER3_BASE + LDR_OFFSET)
#define TIMER3_CTRL (volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET)
#define TIMER3_VAL (volatile uint32_t*)(TIMER3_BASE + VAL_OFFSET)

#define NUM_ITERS 4096 * 4

struct SenderParams {
    size_t msg_size;
    int reciever_tid;
};

void Sender() {
    int first_task_tid;

    SenderParams params;
    Receive(&first_task_tid, (char*)&params, sizeof(params));
    Reply(first_task_tid, nullptr, 0);

    char msg[256];
    char resp[256];

    memset(msg, '.', sizeof(msg));

    for (int i = 0; i < NUM_ITERS; i++) {
        Send(params.reciever_tid, (char*)&msg, params.msg_size, (char*)&resp,
             params.msg_size);
    }

    Send(first_task_tid, nullptr, 0, nullptr, 0);
}

struct RecieverParams {
    size_t msg_size;
};

void Reciever() {
    int first_task_tid;
    int tid;

    RecieverParams params;
    Receive(&first_task_tid, (char*)&params, sizeof(params));
    Reply(first_task_tid, nullptr, 0);

    char buf[256];

    for (int i = 0; i < NUM_ITERS; i++) {
        Receive(&tid, (char*)&buf, params.msg_size);
        Reply(tid, (char*)&buf, params.msg_size);
    }

    Send(first_task_tid, nullptr, 0, nullptr, 0);
}

void FirstUserTask() {
    // init timer 3
    *TIMER3_LDR = 0xffffffff;
    *TIMER3_CTRL = ENABLE_MASK | CLKSEL_MASK;  // free running + 508 kHz

#ifdef NO_OPTIMIZATION
    const char* opt_lvl = "noopt";
#else
    const char* opt_lvl = "opt";
#endif
#ifdef NENABLE_CACHES
    const char* cache_state = "nocache";
#else
    const char* cache_state = "cache";
#endif

    for (char send_priority : {'R', 'S'}) {
        for (size_t msg_size : {4, 64, 256}) {
            bwprintf(COM2, "%s %s %c %d ", opt_lvl, cache_state, send_priority,
                     msg_size);

            int sender_tid = Create(2, Sender);
            int reciever_tid;
            if (send_priority == 'R') {
                // Reciever has higher priority
                reciever_tid = Create(3, Reciever);
            } else {
                // sender has higher priority
                reciever_tid = Create(1, Reciever);
            }

            SenderParams sender_params = {.msg_size = msg_size,
                                          .reciever_tid = reciever_tid};
            RecieverParams reciever_params = {.msg_size = msg_size};

            uint32_t start_time = *TIMER3_VAL;

            Send(sender_tid, (char*)&sender_params, sizeof(sender_params),
                 nullptr, 0);
            Send(reciever_tid, (char*)&reciever_params, sizeof(reciever_params),
                 nullptr, 0);

            // wait for tasks to finish
            int dummy;
            Receive(&dummy, nullptr, 0);
            Reply(dummy, nullptr, 0);
            Receive(&dummy, nullptr, 0);
            Reply(dummy, nullptr, 0);

            uint32_t total_time = start_time - *TIMER3_VAL;
            uint64_t micros = (((uint64_t)total_time) * 1000 / 508) / NUM_ITERS;
            bwprintf(COM2, "%llu (%lu)" ENDL, micros, total_time);
        }
    }
}
