#include "kernel/kernel.h"

namespace kernel::handlers {

// Create calls out to create_task, but ensures that priority is
// non-negative. This lets us enforce that all user tasks have higher
// priority than the kernel's idle task.
int Create(int priority, void* function) {
    kdebug("Called Create(priority=%d, function=%p)", priority, function);
    if (priority < 0) return INVALID_PRIORITY;
    return helpers::create_task(priority, function,
                                /* force_tid */ std::nullopt);
}

}  // namespace kernel::handlers

namespace kernel::helpers {

/// Helper POD struct to init new user task stacks
struct FreshStack {
    uint32_t spsr;
    void* start_addr;
    uint32_t regs[13];
    void* lr;
};

static std::optional<Tid> next_tid() {
    for (size_t tid = 0; tid < MAX_SCHEDULED_TASKS; tid++) {
        if (!tasks[tid].has_value()) return Tid(tid);
    }
    return std::nullopt;
}

int create_task(int priority, void* function, std::optional<Tid> force_tid) {
    std::optional<Tid> fresh_tid = next_tid();
    if (!fresh_tid.has_value()) return OUT_OF_TASK_DESCRIPTORS;
    Tid tid = fresh_tid.value();

    if (force_tid.has_value()) {
        tid = force_tid.value();
    }

    kassert(!tasks[tid].has_value());

    if (ready_queue.push(tid, priority) == PriorityQueueErr::FULL) {
        kpanic("out of space in ready queue (tid=%u)", (size_t)tid);
    }

    // GCC complains that writing *anything* to `stack` is an out-of-bounds
    // error,  because `&__USER_STACKS_START__` is simply a `char*` with no
    // bounds information (and hence, `start_of_stack` also has no bounds
    // information). We know that `start_of_stack` is actually then high
    // address of a block of memory implicitly allocated for the user task
    // stack (with more than enough space for a FreshStack struct), but GCC
    // doesn't, so we must squelch -Warray-bounds.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    // set up memory for the initial user stack
    char* start_of_stack =
        &__USER_STACKS_START__ + (USER_STACK_SIZE * ((size_t)tid + 1));
    if (start_of_stack > &__USER_STACKS_END__) {
        kpanic(
            "Create(): stack overflow! start_of_stack (%p) > "
            "&__USER_STACKS_END__ (%p)",
            start_of_stack, &__USER_STACKS_END__);
    }

    FreshStack* stack =
        (FreshStack*)(void*)(start_of_stack - sizeof(FreshStack));

    stack->spsr = 0x50;
    stack->start_addr = function;
    for (uint32_t i = 0; i < 13; i++)  // set regs to their own vals, for debug
        stack->regs[i] = i;
    stack->lr = (void*)user::Exit;  // implicit Exit() calls!
#pragma GCC diagnostic pop

    kdebug("Created: tid=%u priority=%d function=%p", (size_t)tid, priority,
           function);

    tasks[tid] =
        TaskDescriptor::create(tid, priority, current_task, (void*)stack);
    return tid;
}

}  // namespace kernel::helpers
