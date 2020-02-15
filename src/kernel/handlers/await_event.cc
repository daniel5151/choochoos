#include "common/variant_helpers.h"
#include "kernel/kernel.h"

namespace kernel::handlers {

int AwaitEvent(int eventid) {
    switch (eventid) {
        case 4:
        case 5:
        case 51:
        case 54:
            break;
        default:
            kpanic("AwaitEvent(%d): invalid eventid", eventid);
            return -1;
    }
    kassert(tasks[current_task].has_value());

    if (event_queue.has(eventid)) {
        auto tid_or_volatile_data = event_queue.get(eventid);
        assert(tid_or_volatile_data.has_value());
        int ret = -4;
        std::visit(
            overloaded{
                [&](Tid blocked_tid) {
                    kpanic(
                        "AwaitEvent(%d): tid %u already waiting for this event",
                        eventid, (size_t)blocked_tid);
                },
                [&](VolatileData data) {
                    kdebug("AwaitEvent(%d): data already arrived: 0x%lx",
                           eventid, data.raw());
                    event_queue.take(eventid);
                    ret = data.raw();
                }},
            tid_or_volatile_data.value());
        kassert(tasks[current_task].value().state.tag == TaskState::READY);
        return ret;
    }

    kdebug("AwaitEvent(%d): put tid %u on event_queue", eventid,
           (size_t)current_task);
    event_queue.put(current_task, eventid);
    tasks[current_task].value().state = {.tag = TaskState::EVENT_WAIT,
                                         .event_wait = {}};

    return -3;
}

}  // namespace kernel::handlers
