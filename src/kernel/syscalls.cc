#include <cstring>

#include "kernel/kernel.h"
#include "kernel/asm.h"
#include "priority_queue.h"

#include "bwio.h"

#define USER_STACK_SIZE 0x40000

// TODO optimize this, taking into account our available RAM and the amount of
// stack we need for the kernel.
#define MAX_SCHEDULED_TASKS 16

// defined in the linker script
extern "C" {
    // Designate region of memory to use for user stacks
    extern char __USER_STACKS_START__, __USER_STACKS_SIZE__;
    extern size_t __MAX_USER_STACKS__;
    // Individual task stack sisze
    extern size_t __USER_STACK_SIZE__;
}


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

namespace User {
    #include "user/syscalls.h"
}

/// Helper POD struct to init new user task stacks
struct FreshStack {
    uint32_t dummy_syscall_val;
    uint32_t spsr;
    void* start_addr;
    uint32_t regs [13];
    void* lr;

    static FreshStack init(void (*start_addr)()) {
        FreshStack stack = FreshStack();

        stack.dummy_syscall_val = 0xdeadbeef;
        stack.spsr = 0xc0;
        stack.start_addr = (void*)start_addr;
        for (uint32_t i = 0; i < 13; i++)
            stack.regs[i] = i;
        stack.lr = (void*)User::Exit; // implicit Exit() calls!
        return stack;
    }
};

/// Helper POD struct which can should be casted from a void* that points to a
/// user's stack.
struct SwiUserStack {
    uint32_t spsr;
    void* start_addr;
    uint32_t regs [13];
    void* lr;
    // C++ doesn't support flexible array members, so instead, we use an array
    // of size 1, and just do "OOB" memory access lol
    uint32_t additional_params [1];
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

    int Create(int priority, void (*function)()) {
        if (priority < 0 || priority >= MAX_PRIORITY) return INVALID_PRIORITY;
        int tid = next_tid();
        if (tid < 0) return OUT_OF_TASK_DESCRIPTORS;

        if (ready_queue.push(tid, priority) == PriorityQueueErr::FULL) {
            kpanic("out of space in ready queue (tid=%d", tid);
        }

        // set up memory for the initial user stack
        // TODO: do some shenanigans to enable "implicit returns" via Exit
        FreshStack initial_stack = FreshStack::init(function);
        const size_t INIT_STACK_SIZE = (sizeof(FreshStack));

        char* stack_start = &__USER_STACKS_START__ + (USER_STACK_SIZE * (tid + 1));

        memcpy((stack_start - sizeof(FreshStack)), &initial_stack, sizeof(FreshStack));

        bwprintf(COM2, "Created: %d\r\n", tid);

        tasks[tid] = (TaskDescriptor){
            .priority = priority,
            .active = true,
            .parent_tid = MyTid(),
            .sp = stack_start - sizeof(FreshStack)
        };

        return tid;
    }

    void Exit() {
        int tid = MyTid();
        tasks[tid].active = false;
    }

    void Yield() {
        bwprintf(COM2, "Called Yield\r\n");
    }
}

// ---------------------------------------------

extern "C" int handle_syscall(uint32_t no, SwiUserStack* user_sp) {
    // TODO
    bwprintf(COM2, "Hello from handle_syscall!\n\r");
    bwprintf(COM2, "  Called Syscall %d, SP is at 0x%x!\n\r", no, user_sp);
    return 3;
}

extern void FirstUserTask();
extern void DummyTask();

static void initialize() {
    *((uint32_t*)0x028) = (uint32_t)((void*)_swi_handler);

    int tid = Handlers::Create(4, FirstUserTask);
    if (tid < 0) kpanic("could not create tasks (error code %d)", tid);
}

static int schedule() {
    int tid;
    if (ready_queue.pop(tid) == PriorityQueueErr::EMPTY) return -1;
    return tid;
}

static void activate(int tid) {
    current_task = tid;
    tasks[tid].sp = _activate_task(tasks[tid].sp);

    if (ready_queue.push(tid, tasks[tid].priority) == PriorityQueueErr::FULL) {
        kpanic("out of space in ready queue (tid=%d", tid);
    }
}

int kmain() {
    initialize();

    while (true) {
        int next_task = schedule();
        if (next_task < 0) break;
        activate(next_task);

    }

    bwprintf(COM2, "Goodbye from the kernel!\n\r");

    return 0;
}
