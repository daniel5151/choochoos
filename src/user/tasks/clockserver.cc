#include <climits>
#include <cstdint>

#include "common/priority_queue.h"
#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"

#define USER_TICKS_PER_SEC 100    // 10ms
#define TIMER_TICKS_PER_SEC 2000  // 2 kHz

namespace Clock {

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
    Request msg = {.tag = Request::NotifierTick, .notifier_tick = {}};
    while (true) {
        AwaitEvent(5);
        Send(parent, (char*)&msg, sizeof(msg), nullptr, 0);
    }
}

void Server() {
    debug("clockserver started");
    int notifier_tid = Create(INT_MAX, Notifier);
    assert(notifier_tid >= 0);

    uint32_t current_time = 0;

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
                assert(tid == notifier_tid);
                Reply(tid, nullptr, 0);
                current_time++;

                while (true) {
                    const DelayedTask* delayed_task = pq.peek();
                    if (delayed_task == nullptr) break;
                    if (delayed_task->tick_threshold > current_time) break;

                    pq.pop();
                    debug("waking up tid %d", delayed_task->tid);
                    res = {.tag = Response::Empty, .empty = {}};
                    Reply(delayed_task->tid, (char*)&res, sizeof(res));
                }

                break;
            }
            case Request::Time: {
                debug("Clock::Server: Time");
                assert(current_time < INT_MAX);
                res = {.tag = Response::Time, .time = (int)current_time};
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
                uint32_t tick_threshold = current_time + (uint32_t)req.delay;

                // Delayed tasks with a smaller tick_threshold should be woken
                // up first, so they should have a higher priority.
                int priority = (int)(-tick_threshold);
                debug("enqueing tid=%d, tick_thresh=0x%lx priority=%d", tid,
                      tick_threshold, priority);
                assert(pq.push(DelayedTask(tid, tick_threshold), priority) !=
                       PriorityQueueErr::FULL);

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
