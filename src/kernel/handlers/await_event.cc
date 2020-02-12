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
        kpanic("AwaitEvent(%d): tid %u already waiting for this event", eventid,
               event_queue.get(eventid)->raw_tid());
    }

    kdebug("AwaitEvent(%d): put tid %u on event_queue", eventid,
           (size_t)current_task);
    event_queue.put(current_task, eventid);
    tasks[current_task].value().state = {.tag = TaskState::EVENT_WAIT,
                                         .event_wait = {}};

    return -3;
}

}  // namespace kernel::handlers
