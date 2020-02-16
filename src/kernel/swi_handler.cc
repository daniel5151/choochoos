#include "kernel/kernel.h"
#include "kernel/user_stack.h"

namespace kernel::driver {

void handle_syscall(uint32_t no, void* user_sp) {
    kassert(tasks[current_task].has_value());

    tasks[current_task].value().sp = user_sp;

    UserStack* user_stack = (UserStack*)user_sp;
    std::optional<int> ret = std::nullopt;
    using namespace handlers;
    switch (no) {
        case 0:
            Yield();
            break;
        case 1:
            Exit();
            break;
        case 2:
            ret = MyParentTid();
            break;
        case 3:
            ret = MyTid();
            break;
        case 4:
            ret = Create(user_stack->regs[0], (void*)user_stack->regs[1]);
            break;
        case 5:
            ret = Send(user_stack->regs[0], (const char*)user_stack->regs[1],
                       user_stack->regs[2], (char*)user_stack->regs[3],
                       user_stack->additional_params[0]);
            break;
        case 6:
            ret = Receive((int*)user_stack->regs[0], (char*)user_stack->regs[1],
                          user_stack->regs[2]);
            break;
        case 7:
            ret = Reply(user_stack->regs[0], (const char*)user_stack->regs[1],
                        user_stack->regs[2]);
            break;
        case 8:
            ret = AwaitEvent(user_stack->regs[0]);
            break;
        case 9:
            handlers::Perf((user::perf_t*)user_stack->regs[0]);
            break;
        case 10:
            Panic();
            break;
        default:
            kpanic("invalid syscall %lu", no);
    }
    if (ret.has_value()) {
        TaskDescriptor::write_syscall_return_value(tasks[current_task].value(),
                                                   ret.value());
    }
}

}  // namespace kernel::driver

extern "C" void handle_syscall(uint32_t no, void* user_sp) {
    kernel::driver::handle_syscall(no, user_sp);
}
