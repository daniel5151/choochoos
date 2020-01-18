#include <cstring>

#include "bwio.h"
#include "kernel/asm.h"
#include "kernel/kernel.h"
#include "priority_queue.h"

namespace User {
#include "user/syscalls.h"
}

// defined in the linker script
extern "C" {
// Designate region of memory to use for user stacks
extern char __USER_STACKS_START__, __USER_STACKS_SIZE__;
extern size_t __MAX_USER_STACKS__;
// Individual task stack size
extern size_t __USER_STACK_SIZE__;
}

#define USER_STACK_SIZE 0x40000

// TODO optimize this, taking into account our available RAM and the amount of
// stack we need for the kernel.
#define MAX_SCHEDULED_TASKS 16

#define MAX_PRIORITY 8  // 0 <= priority < 8
#define MAX_TASKS_PER_PRIORITY 8
#define INVALID_PRIORITY -1
#define OUT_OF_TASK_DESCRIPTORS -2

struct TaskDescriptor {
    int priority;
    bool active;
    int parent_tid;

    void* sp;
};

static TaskDescriptor tasks[MAX_SCHEDULED_TASKS];
static PriorityQueue<int /* task descriptor */, MAX_PRIORITY,
                     MAX_TASKS_PER_PRIORITY>
    ready_queue;

static int current_task = -1;

static int next_tid() {
    for (int tid = 0; tid < MAX_SCHEDULED_TASKS; tid++) {
        if (!tasks[tid].active) return tid;
    }
    return -1;
}

namespace Handlers {

int MyTid() { return current_task; }
int MyParentTid() { return tasks[MyTid()].parent_tid; }

/// Helper POD struct to init new user task stacks
struct FreshStack {
    uint32_t dummy_syscall_response;
    void* start_addr;
    uint32_t spsr;
    uint32_t regs[13];
    void* lr;
};

int Create(int priority, void* function) {
    kdebug("Called Create(priority=%d, function=%p)", priority, function);

    if (priority < 0 || priority >= MAX_PRIORITY) return INVALID_PRIORITY;
    int tid = next_tid();
    if (tid < 0) return OUT_OF_TASK_DESCRIPTORS;

    if (ready_queue.push(tid, priority) == PriorityQueueErr::FULL) {
        kpanic("out of space in ready queue (tid=%d)", tid);
    }

    // set up memory for the initial user stack
    char* start_of_stack =
        &__USER_STACKS_START__ + (USER_STACK_SIZE * (tid + 1));

    FreshStack* stack = (FreshStack*)(start_of_stack - sizeof(FreshStack));

    // GCC complains that writing *anything* to `stack` is an out-of-bounds
    // error,  because `&__USER_STACKS_START__` is simply a `char*` with no
    // bounds information (and hence, `start_of_stack` also has no bounds
    // information). We know that `start_of_stack` is actually then high address
    // of a block of memory implicitly allocated for the user task stack (with
    // more than enough space for a FreshStack struct), but GCC doesn't, so we
    // must squelch -Warray-bounds.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    stack->dummy_syscall_response = 0xdeadbeef;
    stack->spsr = 0xc0;
    stack->start_addr = function;
    for (uint32_t i = 0; i < 13; i++)  // set regs to their own vals, for debug
        stack->regs[i] = i;
    stack->lr = (void*)User::Exit;  // implicit Exit() calls!
#pragma GCC diagnostic pop

    kdebug("Created: tid=%d priority=%d function=%p", tid, priority, function);

    tasks[tid] = (TaskDescriptor){.priority = priority,
                                  .active = true,
                                  .parent_tid = MyTid(),
                                  .sp = stack};
    return tid;
}

void Exit() {
    kdebug("Called Exit");
    int tid = MyTid();
    tasks[tid].active = false;
}

void Yield() { kdebug("Called Yield"); }

}  // namespace Handlers

// ---------------------------------------------

/// Helper POD struct which can should be casted from a void* that points to a
/// user's stack.
struct SwiUserStack {
    void* start_addr;
    uint32_t spsr;
    uint32_t regs[13];
    void* lr;
    // C++ doesn't support flexible array members, so instead, we use an array
    // of size 1, and just do "OOB" memory access lol
    uint32_t additional_params[1];
};

extern "C" int handle_syscall(uint32_t no, SwiUserStack& user_sp) {
    switch (no) {
        case 0:
            Handlers::Yield();
            return 0;
        case 1:
            Handlers::Exit();
            return 0;
        case 2:
            return Handlers::MyParentTid();
        case 3:
            return Handlers::MyTid();
        case 4:
            return Handlers::Create(user_sp.regs[0], (void*)user_sp.regs[1]);
        default:
            kpanic("invalid syscall %lu", no);
    }
}

static int schedule() {
    int tid;
    if (ready_queue.pop(tid) == PriorityQueueErr::EMPTY) return -1;
    return tid;
}

static void activate(int tid) {
    current_task = tid;
    void* next_sp = _activate_task(tasks[tid].sp);

    if (tasks[tid].active) {
        tasks[tid].sp = next_sp;
        if (ready_queue.push(tid, tasks[tid].priority) ==
            PriorityQueueErr::FULL) {
            kpanic("out of space in ready queue (tid=%d)", tid);
        }
    }
}

extern void FirstUserTask();

static void initialize() {
    *((uint32_t*)0x028) = (uint32_t)((void*)_swi_handler);

    int tid = Handlers::Create(4, (void*)FirstUserTask);
    if (tid < 0) kpanic("could not create tasks (error code %d)", tid);
}

int kmain() {
    kprintf("Hello from the choochoos kernel!");

    initialize();

    while (true) {
        int next_task = schedule();
        if (next_task < 0) break;
        activate(next_task);
    }

    kprintf("Goodbye from choochoos kernel!");

    return 0;
}
