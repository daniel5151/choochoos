#include <climits>
#include <cstdint>

#include "common/priority_queue.h"
#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/nameserver.h"

#define USER_TICKS_PER_SEC 100    // 10ms
#define TIMER_TICKS_PER_SEC 2000  // 2 kHz

namespace Clock {

class DelayedTask {
   public:
    int tid;
    // make the task ready once *timer3_value_reg < tick_threshold
    int tick_threshold;
    DelayedTask(int tid, int tick_threshold)
        : tid(tid), tick_threshold(tick_threshold) {}
};

static PriorityQueue<DelayedTask, 32> pq;

struct Request {
    enum { Time, Delay, DelayUntil, NotifierTick, Shutdown } tag;
    union {
        struct {
        } time;
        struct {
        } notifier_tick;
        int delay;
        struct {
        } shutdown;
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
    bool shutdown = false;
    while (true) {
        AwaitEvent(5);
        Send(parent, (char*)&msg, sizeof(msg), (char*)&shutdown,
             sizeof(shutdown));
        if (shutdown) {
            debug("shutting down notifier");
            return;
        }
    }
}

void Server() {
    debug("clockserver started");
    int nsres = NameServer::RegisterAs(SERVER_ID);
    assert(nsres >= 0);
    int notifier_tid = Create(INT_MAX, Notifier);
    assert(notifier_tid >= 0);

    int current_time = 0;

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
                bool shutdown = false;
                Reply(tid, (char*)&shutdown, sizeof(shutdown));
                current_time++;

                while (true) {
                    const DelayedTask* hd = pq.peek();
                    if (hd == nullptr) break;
                    if (hd->tick_threshold > current_time) break;

                    // hd != nullptr, so we know pop() returns a value.
                    DelayedTask delayed_task = pq.pop().value();
                    debug("current_time=%d waking up tid %d", current_time,
                          delayed_task.tid);
                    res = {.tag = Response::Empty, .empty = {}};
                    Reply(delayed_task.tid, (char*)&res, sizeof(res));
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
                int tick_threshold = current_time + req.delay;

                // Delayed tasks with a smaller tick_threshold should be woken
                // up first, so they should have a higher priority.
                int priority = -tick_threshold;
                debug("putting tid %d in pq tick_thresh=%d priority=%d", tid,
                      tick_threshold, priority);
                auto err = pq.push(DelayedTask(tid, tick_threshold), priority);
                if (err == PriorityQueueErr::FULL) panic("timer buffer full");
                assert(pq.peek()->tick_threshold <= tick_threshold);

                break;
            }
                // TODO implement DelayUntil
            case Request::Shutdown: {
                while (true) {
                    int tid_;
                    Receive(&tid_, (char*)&req, sizeof(req));
                    if (tid_ != notifier_tid) {
                        panic(
                            "clockserver received user message after "
                            "Shutdown!");
                    }
                    bool shutdown = true;
                    Reply(tid_, (char*)&shutdown, sizeof(shutdown));
                    Reply(tid, nullptr, 0);
                    debug("shutting down clockserver");
                    return;
                }
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

void Shutdown(int clockserver) {
    Request req = {.tag = Request::Shutdown, .shutdown = {}};
    Send(clockserver, (char*)&req, sizeof(req), nullptr, 0);
}

}  // namespace Clock
