#include "kernel/kernel.h"
#include "kernel/user_stack.h"

namespace kernel {

TaskDescriptor TaskDescriptor::create(Tid tid,
                                      size_t priority,
                                      std::optional<Tid> parent_tid,
                                      void* stack_ptr) {
    return {.tid = tid,
            .send_queue_head = std::nullopt,
            .send_queue_tail = std::nullopt,
            .priority = priority,
            .state = {.tag = TaskState::READY, .ready = {}},
            .parent_tid = parent_tid,
            .sp = stack_ptr};
}

void TaskDescriptor::write_syscall_return_value(TaskDescriptor& task,
                                                int32_t value) {
    UserStack* stack = (UserStack*)task.sp;
    *((int32_t*)&stack->regs[0]) = value;
}

}  // namespace kernel
