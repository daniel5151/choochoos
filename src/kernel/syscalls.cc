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

#define INVALID_PRIORITY -1
#define OUT_OF_TASK_DESCRIPTORS -2

static inline int min(int a, int b) { return a < b ? a : b; }

namespace kernel {

enum class TaskState { UNUSED, READY, BLOCKED_ON_RECEIVE };

struct Message {
    int tid;
    const char* msg;
    int msglen;
};

typedef Queue<Message, 16> Mailbox;

class TaskDescriptor {
   public:
    size_t priority;
    TaskState state;
    int parent_tid;

    void* sp;

    // message passing state
    int* recv_tid;
    char* recv_buf;
    size_t recv_buf_len;
    Mailbox mailbox;

    TaskDescriptor() : state(TaskState::UNUSED), parent_tid(-2) {}

    TaskDescriptor(size_t priority, int parent_tid, void* stack_ptr)
        : priority(priority),
          state(TaskState::READY),
          parent_tid(parent_tid),
          sp(stack_ptr),
          recv_tid(nullptr),
          recv_buf(nullptr),
          recv_buf_len(0),
          mailbox() {}

    void reset() {
        state = TaskState::UNUSED;
        sp = nullptr;
        parent_tid = -2;
        recv_tid = nullptr;
        recv_buf = nullptr;
        recv_buf_len = 0;
        mailbox.clear();
    }
};

class Kernel {
    /// Helper POD struct to init new user task stacks
    struct FreshStack {
        uint32_t dummy_syscall_response;
        void* start_addr;
        uint32_t spsr;
        uint32_t regs[13];
        void* lr;
    };

    TaskDescriptor tasks[MAX_SCHEDULED_TASKS];
    PriorityQueue<int /* task descriptor */, MAX_SCHEDULED_TASKS> ready_queue;

    int current_task = -1;

    int next_tid() {
        for (int tid = 0; tid < MAX_SCHEDULED_TASKS; tid++) {
            if (tasks[tid].state == TaskState::UNUSED) return tid;
        }
        return -1;
    }

    // ------------------ syscall handlers ----------------------

    int MyTid() { return current_task; }
    int MyParentTid() { return tasks[MyTid()].parent_tid; }

    int Create(int priority, void* function) {
        kdebug("Called Create(priority=%d, function=%p)", priority, function);

        if (priority < 0) return INVALID_PRIORITY;
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
        // information). We know that `start_of_stack` is actually then high
        // address of a block of memory implicitly allocated for the user task
        // stack (with more than enough space for a FreshStack struct), but GCC
        // doesn't, so we must squelch -Warray-bounds.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
        stack->dummy_syscall_response = 0xdeadbeef;
        stack->spsr = 0xc0;
        stack->start_addr = function;
        for (uint32_t i = 0; i < 13;
             i++)  // set regs to their own vals, for debug
            stack->regs[i] = i;
        stack->lr = (void*)User::Exit;  // implicit Exit() calls!
#pragma GCC diagnostic pop

        kdebug("Created: tid=%d priority=%d function=%p", tid, priority,
               function);

        tasks[tid] = TaskDescriptor((size_t)priority, MyTid(), (void*)stack);
        return tid;
    }

    void Exit() {
        kdebug("Called Exit");
        int tid = MyTid();
        tasks[tid].reset();
    }

    void Yield() { kdebug("Called Yield"); }

    int Send(int receiver_tid, const char* msg, int msglen, char* reply,
             int rplen) {
        kdebug("Called Send(tid=%d msg=%s msglen=%d reply=%p rplen=%d)",
               receiver_tid, msg, msglen, reply, rplen);
        if (receiver_tid < 0 || receiver_tid >= MAX_SCHEDULED_TASKS)
            return -1;  // invalid tid
        int sender_tid = MyTid();
        auto sender = &tasks[sender_tid];
        auto receiver = &tasks[receiver_tid];
        switch (receiver->state) {
            case TaskState::UNUSED:
                return -1;  // no running task with tid
            case TaskState::READY: {
                Message message = {
                    .tid = sender_tid,
                    .msg = msg,
                    .msglen = msglen,
                };

                if (receiver->mailbox.push_back(message) == QueueErr::FULL) {
                    kdebug("mailbox full for task %d", receiver_tid);
                    return -2;
                }

                break;
            }
            case TaskState::BLOCKED_ON_RECEIVE: {
                memcpy(receiver->recv_buf, msg,
                       min(msglen, receiver->recv_buf_len));
                receiver->state = TaskState::READY;
                if (receiver->recv_tid != nullptr) {
                    *(receiver->recv_tid) = sender_tid;
                }

                ready_queue.push(receiver_tid, receiver->priority);

                break;
            }
            default:
                kpanic("invalid state %d for task %d",
                       (int)tasks[receiver_tid].state, receiver_tid);
        }

        // Send() doesn't care which tid sends the reply
        sender->recv_tid = nullptr;
        sender->recv_buf = reply;
        sender->recv_buf_len = (size_t)rplen;
        sender->state = TaskState::BLOCKED_ON_RECEIVE;

        return 0;
    }

    int Receive(int* tid, char* msg, int msglen) {
        kdebug("Called Receive(tid=%p msg=%p msglen=%d)", tid, msg, msglen);

        auto task = &tasks[MyTid()];

        if (task->mailbox.is_empty()) {
            task->recv_tid = tid;
            task->recv_buf = msg;
            task->recv_buf_len = msglen;
            task->state = TaskState::BLOCKED_ON_RECEIVE;
            return 0;
        }

        Message m{.tid = -1, .msg = nullptr, .msglen = 0};
        task->mailbox.pop_front(m);
        *tid = m.tid;
        memcpy(msg, m.msg, min(msglen, m.msglen));
        return 0;
    }

    int Reply(int tid, const char* reply, int rplen) {
        kdebug("Called Reply(tid=%d reply=%p rplen=%d)", tid, reply, rplen);
        if (tid < 0 || tid >= MAX_SCHEDULED_TASKS) return -1;
        int sender_tid = MyTid();
        auto receiver = &tasks[tid];
        switch (receiver->state) {
            case TaskState::UNUSED:
                return -1;
            case TaskState::BLOCKED_ON_RECEIVE:
                memcpy(receiver->recv_buf, reply,
                       min(receiver->recv_buf_len, rplen));
                if (receiver->recv_tid != nullptr) {
                    *(receiver->recv_tid) = sender_tid;
                }
                receiver->state = TaskState::READY;
                ready_queue.push(tid, receiver->priority);
                return 0;
            case TaskState::READY: {
                Message message =
                    (Message){.tid = sender_tid, .msg = reply, .msglen = rplen};
                if (receiver->mailbox.push_back(message) == QueueErr::FULL) {
                    return -2;
                }
                return 0;
            }
            default:
                kpanic("invalid state %d for task %d", (int)receiver->state,
                       tid);
        }
    }

    /// Helper POD struct which can should be casted from a void* that points to
    /// a user's stack.
    struct SwiUserStack {
        void* start_addr;
        uint32_t spsr;
        uint32_t regs[13];
        void* lr;
        // C++ doesn't support flexible array members, so instead, we use an
        // array of size 1, and just do "OOB" memory access lol
        uint32_t additional_params[1];
    };

   public:
    Kernel() : tasks{TaskDescriptor()}, ready_queue() {}

    int handle_syscall(uint32_t no, void* user_sp) {
        SwiUserStack* user_stack = (SwiUserStack*)user_sp;
        switch (no) {
            case 0:
                Yield();
                return 0;
            case 1:
                Exit();
                return 0;
            case 2:
                return MyParentTid();
            case 3:
                return MyTid();
            case 4:
                return Create(user_stack->regs[0], (void*)user_stack->regs[1]);
            case 5:
                return Send(user_stack->regs[0],
                            (const char*)user_stack->regs[1],
                            user_stack->regs[2], (char*)user_stack->regs[3],
                            user_stack->additional_params[0]);
            case 6:
                return Receive((int*)user_stack->regs[0],
                               (char*)user_stack->regs[1], user_stack->regs[2]);
            case 7:
                return Reply(user_stack->regs[0],
                             (const char*)user_stack->regs[1],
                             user_stack->regs[2]);
            default:
                kpanic("invalid syscall %lu", no);
        }
    }

    int schedule() {
        int tid;
        if (ready_queue.pop(tid) == PriorityQueueErr::EMPTY) return -1;
        return tid;
    }

    void activate(int tid) {
        current_task = tid;
        tasks[tid].sp = _activate_task(tasks[tid].sp);

        switch (tasks[tid].state) {
            case TaskState::READY:
                if (ready_queue.push(tid, tasks[tid].priority) ==
                    PriorityQueueErr::FULL) {
                    kpanic("out of space in ready queue (tid=%d)", tid);
                }
                break;
            case TaskState::BLOCKED_ON_RECEIVE:
            case TaskState::UNUSED:
                break;
        }
    }

    void initialize(void (*user_main)()) {
        *((uint32_t*)0x028) = (uint32_t)((void*)_swi_handler);

        int tid = Create(4, (void*)user_main);
        if (tid < 0) kpanic("could not create tasks (error code %d)", tid);
    }
};  // class Kernel
}  // namespace kernel

extern void FirstUserTask();

static kernel::Kernel kern;

extern "C" int handle_syscall(uint32_t no, void* user_sp) {
    return kern.handle_syscall(no, user_sp);
}

int kmain() {
    kprintf("Hello from the choochoos kernel!");

    kern.initialize(FirstUserTask);

    while (true) {
        int next_task = kern.schedule();
        if (next_task < 0) break;
        kern.activate(next_task);
    }

    kprintf("Goodbye from choochoos kernel!");

    return 0;
}
