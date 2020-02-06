#include <climits>
#include <cstdint>

#include "common/priority_queue.h"
#include "common/ts7200.h"
#include "user/dbg.h"
#include "user/syscalls.h"

#define USER_TICKS_PER_SEC 100    // 10ms
#define TIMER_TICKS_PER_SEC 2000  // 2 kHz

namespace Clock {

// TODO if we're going to expose peripheral registers to this user task, we
// should encapsulate them in a header file, with appropriate constness.
static volatile uint32_t* timer2_load_reg =
    (volatile uint32_t*)(TIMER2_BASE + LDR_OFFSET);
static volatile uint32_t* timer2_ctrl_reg =
    (volatile uint32_t*)(TIMER2_BASE + CRTL_OFFSET);

static volatile uint32_t* timer3_value_reg =
    (volatile uint32_t*)(TIMER3_BASE + VAL_OFFSET);

class DelayedTask {
   public:
    int tid;
    // make the task ready once *timer3_value_reg < tick_threshold
    uint32_t tick_threshold;
    DelayedTask(int tid, uint32_t tick_threshold)
        : tid(tid), tick_threshold(tick_threshold) {}
};

static PriorityQueue<DelayedTask, 32> pq;

struct Request {
    enum { Time, Delay, DelayUntil, NotifierTick } tag;
    union {
        struct {
        } time;
        struct {
        } notifier_tick;
        int delay;
    };
};

struct Response {
    enum { Time, Empty } tag;
    union {
        int time;
        struct {
        } empty;
    };
};

void Notifier() {
    debug("clockserver notifier started");
    int parent = MyParentTid();
    Request msg;
    while (true) {
        AwaitEvent(5);
        debug("clockserver notifier sending tick");
        msg = {.tag = Request::NotifierTick, .notifier_tick = {}};
        Send(parent, (char*)&msg, sizeof(msg), nullptr, 0);
    }
}

static void schedule_timer_interrupt(uint16_t ticks) {
    debug("scheduling delay in %d timer ticks", ticks);
    *timer2_ctrl_reg = 0;
    *timer2_load_reg = (uint32_t)ticks;
    // periodic + 2 kHz
    *timer2_ctrl_reg = ENABLE_MASK | MODE_MASK;
}

static void stop_timer_interrupts() { *timer2_ctrl_reg = 0; }

void Server() {
    debug("clockserver started");
    int notifier_tid = Create(INT_MAX, Notifier);
    assert(notifier_tid >= 0);

    int tid;
    Request req;
    Response res;
    while (true) {
        int n = Receive(&tid, (char*)&req, sizeof(req));
        if (n < 0) panic("Clock::Server: bad Receive() n=%d", n);
        if (n != sizeof(req))
            panic("Clock::Server: Receive() wrong size n=%d expected=%d", n,
                  sizeof(req));

        switch (req.tag) {
            case Request::NotifierTick: {
                stop_timer_interrupts();
                assert(tid == notifier_tid);
                debug("Clock::Server: NotifierTick current_time=0x%lx",
                      *timer3_value_reg);
                Reply(tid, nullptr, 0);

                debug("%d tasks are queued", pq.size());
                while (true) {
                    const DelayedTask* delayed_task = pq.peek();
                    if (delayed_task == nullptr) break;
                    if (delayed_task->tick_threshold < *timer3_value_reg) break;

                    pq.pop();
                    debug("waking up tid %d", delayed_task->tid);
                    res = {.tag = Response::Empty, .empty = {}};
                    Reply(delayed_task->tid, (char*)&res, sizeof(res));
                }

                const DelayedTask* delayed_task = pq.peek();
                if (delayed_task != nullptr) {
                    uint16_t timer_ticks = (uint16_t)std::min(
                        (uint32_t)UINT16_MAX,
                        *timer3_value_reg - delayed_task->tick_threshold);
                    schedule_timer_interrupt(timer_ticks);
                }

                break;
            }
            case Request::Time: {
                debug("Clock::Server: Time");
                int timer3_ticks = (int)(UINT32_MAX - *timer3_value_reg);
                int user_ticks =
                    timer3_ticks * USER_TICKS_PER_SEC / TIMER_TICKS_PER_SEC;
                res = {.tag = Response::Time, .time = user_ticks};
                Reply(tid, (char*)&res, sizeof(res));
                break;
            }
            case Request::Delay: {
                debug("Clock::Server: Delay(%d)", req.delay);
                if (req.delay <= 0) {
                    res = {.tag = Response::Empty, .empty = {}};
                    Reply(tid, (char*)&res, sizeof(res));
                    break;
                }
                uint32_t user_delay = (uint32_t)std::max(0, req.delay);
                uint16_t timer_delay = (uint16_t)std::min(
                    user_delay * TIMER_TICKS_PER_SEC / USER_TICKS_PER_SEC,
                    (uint32_t)UINT16_MAX);
                uint32_t tick_threshold = *timer3_value_reg - timer_delay;
                int priority = (int)(tick_threshold - UINT32_MAX);
                debug("enqueing tid=%d, tick_thresh=0x%lx priority=%d", tid,
                      tick_threshold, priority);
                // TODO only schedule a delay if tick_threshold >
                // pq.peek()->tick_threshold.
                assert(pq.push(DelayedTask(tid, tick_threshold), priority) !=
                       PriorityQueueErr::FULL);
                schedule_timer_interrupt(timer_delay);

                break;
            }
            default:
                panic("Clock::Server: Receive() bad tag %d", (int)req.tag);
        }
    }
}

int Time(int clockserver) {
    Request req = {.tag = Request::Time, .time = {}};
    Response res;
    int n =
        Send(clockserver, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) return -1;
    if (res.tag != Response::Time) return -1;
    return res.time;
}

int Delay(int clockserver, int ticks) {
    Request req = {.tag = Request::Delay, .delay = ticks};
    Response res;
    int n =
        Send(clockserver, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (n != sizeof(res)) return -1;
    if (res.tag != Response::Empty) return -1;
    return 0;
}

}  // namespace Clock
