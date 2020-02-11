#include <cstring>  // memcpy

#include "kernel/kernel.h"

namespace kernel {

int Kernel::Reply(int tid, const char* reply, int rplen) {
    kdebug("Called Reply(tid=%d reply=%p rplen=%d)", tid, reply, rplen);
    if (tid < 0 || tid >= MAX_SCHEDULED_TASKS) return -1;
    if (!tasks[tid].has_value()) return -1;
    TaskDescriptor& receiver = tasks[tid].value();
    switch (receiver.state.tag) {
        case TaskState::REPLY_WAIT: {
            size_t n = std::min(receiver.state.reply_wait.rplen,
                                (size_t)std::max(rplen, 0));
            memcpy(receiver.state.reply_wait.reply, reply, n);
            receiver.state = {.tag = TaskState::READY, .ready = {}};
            ready_queue.push(tid, receiver.priority);

            // Return the length of the reply to the original sender.
            //
            // The receiver of the reply is blocked, so the stack pointer
            // in the TaskDescriptor points at the top of the stack. Since
            // the top of the stack represents the syscall return word, we
            // can write directly to the stack pointer.
            TaskDescriptor::write_syscall_return_value(receiver, (int32_t)n);

            // Return the length of the reply to the original receiver.
            return (int)n;
        }
        default:
            return -2;
    }
}

}  // namespace kernel
