#include "kernel/kernel.h"

namespace kernel {

void Kernel::Exit() {
    kdebug("Called Exit");
    Tid tid = current_task;
    kassert(tasks[tid].has_value());
    reset_task(tasks[tid].value());
    tasks[tid] = std::nullopt;
}

void Kernel::reset_task(TaskDescriptor& task) {
    task.sp = nullptr;
    task.parent_tid = std::nullopt;

    if (!task.send_queue_head.has_value()) return;

    Tid tid = task.send_queue_head.value();

    task.send_queue_head = std::nullopt;
    task.send_queue_tail = std::nullopt;

    while (true) {
        kassert(tasks[tid].has_value());

        auto& task = tasks[tid].value();
        kassert(task.state.tag == TaskState::SEND_WAIT);
        std::optional<Tid> next_tid = task.state.send_wait.next;

        kdebug("tid=%u cannot complete SRR, receiver (%u) shut down",
               (size_t)this->tid, (size_t)tid);

        // SRR could not be completed, return -2 to the sender
        TaskDescriptor::write_syscall_return_value(task, -2);
        task.state = {.tag = TaskState::READY, .ready = {}};
        if (ready_queue.push(tid, task.priority) != PriorityQueueErr::OK) {
            kpanic("ready queue full");
        }

        if (!next_tid.has_value()) break;

        tid = next_tid.value();
    }
}

}  // namespace kernel
