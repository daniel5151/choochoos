#include <cstring>
#include <optional>

#include "common/bwio.h"
#include "common/priority_queue.h"
#include "common/ts7200.h"
#include "kernel/asm.h"
#include "kernel/kernel.h"

#include "user/syscalls.h"

// defined in the linker script
extern "C" {
// Designate region of memory to use for user stacks
extern char __USER_STACKS_START__, __USER_STACKS_END__;
}

#define USER_STACK_SIZE 0x40000

#define MAX_SCHEDULED_TASKS 48

#define INVALID_PRIORITY -1
#define OUT_OF_TASK_DESCRIPTORS -2

/// Helper POD struct which can should be casted from a void* that points to
/// a user's stack.
struct SwiUserStack {
    uint32_t spsr;
    uint32_t regs[13];
    void* lr;
    void* start_addr;
    // C++ doesn't support flexible array members, so instead, we use an
    // array of size 1, and just do "OOB" memory access lol
    uint32_t additional_params[1];
};

class Tid {
    size_t id;

   public:
    operator size_t() const { return this->id; }

    Tid(size_t id) : id{id} {}
    int raw_tid() const { return this->id; }
};

struct TaskState {
    enum uint8_t {
        UNUSED,
        READY,
        SEND_WAIT,
        RECV_WAIT,
        REPLY_WAIT,
        EVENT_WAIT
    } tag;
    union {
        struct {
        } unused;
        struct {
        } ready;
        struct {
            const char* msg;
            size_t msglen;
            char* reply;
            size_t rplen;
            std::optional<Tid> next;
        } send_wait;
        struct {
            int* tid;
            char* recv_buf;
            size_t len;
        } recv_wait;
        struct {
            char* reply;
            size_t rplen;
        } reply_wait;
        struct {
        } event_wait;
    };
};

class TaskDescriptor {
    Tid tid;
    std::optional<Tid> send_queue_head;
    std::optional<Tid> send_queue_tail;

    // hack since Kdebug relies on a local MyTid() (TODO plsfix)
    int MyTid() const { return tid.raw_tid(); }

   public:
    size_t priority;
    TaskState state;
    std::optional<Tid> parent_tid;

    void* sp;

    TaskDescriptor() = delete;

    TaskDescriptor(Tid tid,
                   size_t priority,
                   std::optional<Tid> parent_tid,
                   void* stack_ptr)
        : tid{tid},
          send_queue_head{std::nullopt},
          send_queue_tail{std::nullopt},
          priority{priority},
          state{.tag = TaskState::READY, .ready = {}},
          parent_tid{parent_tid},
          sp{stack_ptr} {}

    bool send_queue_is_empty() const { return !send_queue_head.has_value(); }

    void reset(std::optional<TaskDescriptor> (&tasks)[MAX_SCHEDULED_TASKS],
               PriorityQueue<Tid, MAX_SCHEDULED_TASKS>& ready_queue) {
        state = {.tag = TaskState::UNUSED, .unused = {}};
        sp = nullptr;
        parent_tid = std::nullopt;

        if (!send_queue_head.has_value()) return;

        Tid tid = send_queue_head.value();

        send_queue_head = std::nullopt;
        send_queue_tail = std::nullopt;

        while (true) {
            kassert(tasks[tid].has_value());

            auto& task = tasks[tid].value();
            kassert(task.state.tag == TaskState::SEND_WAIT);
            std::optional<Tid> next_tid = task.state.send_wait.next;

            kdebug("tid=%u cannot complete SRR, receiver (%u) shut down",
                   (size_t)this->tid, (size_t)tid);

            // SRR could not be completed, return -2 to the sender
            task.write_syscall_return_value(-2);
            task.state = {.tag = TaskState::READY, .ready = {}};
            if (ready_queue.push(tid, task.priority) != PriorityQueueErr::OK) {
                kpanic("ready queue full");
            }

            if (!next_tid.has_value()) break;

            tid = next_tid.value();
        }
    }

    void add_to_send_queue(
        TaskDescriptor& task,
        const char* msg,
        size_t msglen,
        char* reply,
        size_t rplen,
        std::optional<TaskDescriptor> (&tasks)[MAX_SCHEDULED_TASKS]) {
        kassert(this->state.tag != TaskState::RECV_WAIT);
        kassert(task.state.tag == TaskState::READY);

        task.state = {.tag = TaskState::SEND_WAIT,
                      .send_wait = {.msg = msg,
                                    .msglen = msglen,
                                    .reply = reply,
                                    .rplen = rplen,
                                    .next = std::nullopt}};
        if (send_queue_is_empty()) {
            kassert(!send_queue_head.has_value());
            kassert(!send_queue_tail.has_value());

            send_queue_head = task.tid;
            send_queue_tail = task.tid;
        } else {
            kassert(send_queue_head.has_value());
            kassert(send_queue_tail.has_value());

            kassert(tasks[send_queue_tail.value()].has_value());

            TaskDescriptor& old_tail = tasks[send_queue_tail.value()].value();
            kassert(old_tail.state.tag == TaskState::SEND_WAIT);
            kassert(!old_tail.state.send_wait.next.has_value());
            old_tail.state.send_wait.next = task.tid;
            send_queue_tail = task.tid;
        }
    }

    Tid pop_from_send_queue(
        int* sender_tid,
        char* recv_buf,
        size_t len,
        std::optional<TaskDescriptor> (&tasks)[MAX_SCHEDULED_TASKS]) {
        kassert(!send_queue_is_empty());
        kassert(state.tag == TaskState::READY);
        kassert(send_queue_head.has_value());

        kassert(tasks[send_queue_head.value()].has_value());

        TaskDescriptor& task = tasks[send_queue_head.value()].value();
        kassert(task.state.tag == TaskState::SEND_WAIT);

        size_t n = std::min(task.state.send_wait.msglen, len);
        memcpy(recv_buf, task.state.send_wait.msg, n);
        *sender_tid = send_queue_head.value();

        char* reply = task.state.send_wait.reply;
        size_t rplen = task.state.send_wait.rplen;
        std::optional<Tid> next = task.state.send_wait.next;
        task.state = {.tag = TaskState::REPLY_WAIT,
                      .reply_wait = {reply, rplen}};

        this->state = {.tag = TaskState::READY, .ready = {}};

        send_queue_head = next;
        if (!send_queue_head.has_value()) {
            send_queue_tail = std::nullopt;
        } else {
            kassert(send_queue_tail.has_value());
        }

        return n;
    }

    void write_syscall_return_value(int32_t value) {
        SwiUserStack* stack = (SwiUserStack*)sp;
        *((int32_t*)&stack->regs[0]) = value;
    }
};

static size_t current_interrupt() {
    uint32_t vic1_bits = *((uint32_t*)VIC1_BASE + VIC_IRQ_STATUS_OFFSET);

    for (size_t i = 0; i < 32; i++) {
        if (vic1_bits & (1 << i)) {
            return i;
        }
    }

    uint32_t vic2_bits = *((uint32_t*)VIC2_BASE + VIC_IRQ_STATUS_OFFSET);

    for (size_t i = 0; i < 32; i++) {
        if (vic2_bits & (1 << i)) {
            return 32 + i;
        }
    }

    kpanic("no interrupts are set");
}

class Kernel {
    /// Helper POD struct to init new user task stacks
    struct FreshStack {
        uint32_t spsr;
        uint32_t regs[13];
        void* lr;
        void* start_addr;
    };

    std::optional<TaskDescriptor> tasks[MAX_SCHEDULED_TASKS];
    std::optional<Tid> event_queue[64];
    PriorityQueue<Tid, MAX_SCHEDULED_TASKS> ready_queue;

    Tid current_task = -1;

    std::optional<Tid> next_tid() {
        for (size_t tid = 0; tid < MAX_SCHEDULED_TASKS; tid++) {
            if (!tasks[tid].has_value()) return Tid(tid);
        }
        return std::nullopt;
    }

    // ------------------ syscall handlers ----------------------

    int MyTid() { return current_task; }

    int MyParentTid() {
        if (!tasks[current_task].has_value()) return -1;
        return tasks[current_task].value().parent_tid.value_or(-1);
    }

    int Create(int priority, void* function) {
        kdebug("Called Create(priority=%d, function=%p)", priority, function);

        if (priority < 0) return INVALID_PRIORITY;
        std::optional<Tid> fresh_tid = next_tid();
        if (!fresh_tid.has_value()) return OUT_OF_TASK_DESCRIPTORS;
        Tid tid = fresh_tid.value();

        if (ready_queue.push(tid, priority) == PriorityQueueErr::FULL) {
            kpanic("out of space in ready queue (tid=%u)", (size_t)tid);
        }

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

        // GCC complains that writing *anything* to `stack` is an out-of-bounds
        // error,  because `&__USER_STACKS_START__` is simply a `char*` with no
        // bounds information (and hence, `start_of_stack` also has no bounds
        // information). We know that `start_of_stack` is actually then high
        // address of a block of memory implicitly allocated for the user task
        // stack (with more than enough space for a FreshStack struct), but GCC
        // doesn't, so we must squelch -Warray-bounds.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
        stack->spsr = 0x50;
        stack->start_addr = function;
        for (uint32_t i = 0; i < 13;
             i++)  // set regs to their own vals, for debug
            stack->regs[i] = i;
        stack->lr = (void*)::Exit;  // implicit Exit() calls!
#pragma GCC diagnostic pop

        kdebug("Created: tid=%u priority=%d function=%p", (size_t)tid, priority,
               function);

        tasks[tid] =
            TaskDescriptor(tid, (size_t)priority, current_task, (void*)stack);
        return tid;
    }

    void Exit() {
        kdebug("Called Exit");
        Tid tid = current_task;
        kassert(tasks[tid].has_value());
        tasks[tid]->reset(tasks, ready_queue);
        tasks[tid] = std::nullopt;
    }

    void Yield() { kdebug("Called Yield"); }

    int Send(
        int receiver_tid, const char* msg, int msglen, char* reply, int rplen) {
        kdebug("Called Send(tid=%d msg=%p msglen=%d reply=%p rplen=%d)",
               receiver_tid, msg, msglen, reply, rplen);
        if (receiver_tid < 0 || receiver_tid >= MAX_SCHEDULED_TASKS)
            return -1;  // invalid tid
        if (!tasks[receiver_tid].has_value()) return -1;

        Tid sender_tid = current_task;
        TaskDescriptor& sender = tasks[sender_tid].value();
        TaskDescriptor& receiver = tasks[receiver_tid].value();
        switch (receiver.state.tag) {
            case TaskState::UNUSED:
                return -1;  // no running task with tid
            case TaskState::SEND_WAIT:
            case TaskState::REPLY_WAIT:
            case TaskState::READY: {
                receiver.add_to_send_queue(sender, msg,
                                           (size_t)std::max(msglen, 0), reply,
                                           (size_t)std::max(rplen, 0), tasks);

                // the sender should never see this - it should be overwritten
                // by Reply()
                return -4;
            }
            case TaskState::RECV_WAIT: {
                size_t n = std::min((size_t)std::max(msglen, 0),
                                    receiver.state.recv_wait.len);
                memcpy(receiver.state.recv_wait.recv_buf, msg, n);
                *receiver.state.recv_wait.tid = sender_tid;

                receiver.state = {.tag = TaskState::READY, .ready = {}};
                ready_queue.push(receiver_tid, receiver.priority);

                // set the return value that the receiver gets from Receive() to
                // n.
                receiver.write_syscall_return_value((int32_t)n);

                sender.state = {.tag = TaskState::REPLY_WAIT,
                                .reply_wait = {reply, (size_t)rplen}};
                // the sender should never see this - it should be overwritten
                // by Reply()
                return -3;
            }

            default:
                kpanic("invalid state %d for task %d", (int)receiver.state.tag,
                       receiver_tid);
        }
    }

    int Receive(int* tid, char* msg, int msglen) {
        kdebug("Called Receive(tid=%p msg=%p msglen=%d)", (void*)tid, msg,
               msglen);

        TaskDescriptor& task = tasks[current_task].value();

        switch (task.state.tag) {
            case TaskState::READY: {
                if (task.send_queue_is_empty()) {
                    task.state = {.tag = TaskState::RECV_WAIT,
                                  .recv_wait = {tid, msg, (size_t)msglen}};
                    // this will be overwritten when a sender shows up
                    return -3;
                }

                return task.pop_from_send_queue(
                    tid, msg, (size_t)std::max(msglen, 0), tasks);
            }
            default:
                kdebug("Receive() called from task in non-ready state %d",
                       task.state.tag);
                return -1;
        }
    }

    int Reply(int tid, const char* reply, int rplen) {
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
                receiver.write_syscall_return_value((int32_t)n);

                // Return the length of the reply to the original receiver.
                return (int)n;
            }
            case TaskState::UNUSED:
                return -1;
            default:
                return -2;
        }
    }

   public:
    Kernel() : tasks{std::nullopt}, event_queue{std::nullopt}, ready_queue() {}

    void handle_syscall(uint32_t no, void* user_sp) {
        kassert(tasks[current_task].has_value());

        tasks[current_task].value().sp = user_sp;

        SwiUserStack* user_stack = (SwiUserStack*)user_sp;
        std::optional<int> ret = std::nullopt;
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
                ret =
                    Send(user_stack->regs[0], (const char*)user_stack->regs[1],
                         user_stack->regs[2], (char*)user_stack->regs[3],
                         user_stack->additional_params[0]);
                break;
            case 6:
                ret = Receive((int*)user_stack->regs[0],
                              (char*)user_stack->regs[1], user_stack->regs[2]);
                break;
            case 7:
                ret =
                    Reply(user_stack->regs[0], (const char*)user_stack->regs[1],
                          user_stack->regs[2]);
                break;
            default:
                kpanic("invalid syscall %lu", no);
        }
        if (ret.has_value()) {
            tasks[current_task].value().write_syscall_return_value(ret.value());
        }
    }

    void handle_interrupt(void* user_sp) {
        uint32_t no = current_interrupt();

        kdebug("handle_interrupt: no=%lu user_sp=%p", no, user_sp);

        kassert(tasks[current_task].has_value());
        kassert(no < 64);

        // assert interrupt
        switch (no) {
            case 4:
                *(volatile uint32_t*)(TIMER1_BASE + CLR_OFFSET) = 1;
            case 5:
                *(volatile uint32_t*)(TIMER2_BASE + CLR_OFFSET) = 1;
            case 51:
                *(volatile uint32_t*)(TIMER3_BASE + CLR_OFFSET) = 1;
                break;
            default:
                kpanic("unexpected interrupt number (%d)", no);
        }

        // current_task was preempted, so update its stack pointer and put it
        // back on the ready_queue
        TaskDescriptor& preempted_task = tasks[current_task].value();
        preempted_task.sp = user_sp;

        if (ready_queue.push(current_task, preempted_task.priority) ==
            PriorityQueueErr::FULL) {
            kpanic("ready queue full");
        }

        // if nobody is waiting for the interrupt, drop it
        // TODO should we buffer it?
        if (!event_queue[no].has_value()) return;

        Tid blocked_tid = event_queue[no].value();
        kassert(tasks[blocked_tid].has_value());
        TaskDescriptor& blocked_task = tasks[blocked_tid].value();
        kassert(blocked_task.state.tag = TaskState::EVENT_WAIT);
        blocked_task.state = {.tag = TaskState::READY, .ready = {}};

        if (ready_queue.push(blocked_tid, blocked_task.priority) ==
            PriorityQueueErr::FULL) {
            kpanic("ready queue full");
        }
    }

    std::optional<Tid> schedule() { return ready_queue.pop(); }

    void activate(Tid tid) {
        current_task = tid;
        TaskDescriptor& task = tasks[tid].value();
        task.sp = _activate_task(task.sp);

        switch (task.state.tag) {
            case TaskState::READY:
                if (ready_queue.push(tid, task.priority) ==
                    PriorityQueueErr::FULL) {
                    kpanic("out of space in ready queue (tid=%u)", (size_t)tid);
                }
                break;
            case TaskState::SEND_WAIT:
            case TaskState::RECV_WAIT:
            case TaskState::REPLY_WAIT:
            case TaskState::EVENT_WAIT:
            case TaskState::UNUSED:
                break;
        }
    }

    void initialize(void (*user_main)()) {
        *((uint32_t*)0x028) = (uint32_t)((void*)_swi_handler);
        *((uint32_t*)0x038) = (uint32_t)((void*)_irq_handler);

        // disable protection
        *(volatile uint32_t*)(VIC1_BASE + VIC_INT_PROTECTION_OFFSET) = 0;
        // all IRQs
        *(volatile uint32_t*)(VIC1_BASE + VIC_INT_SELECT_OFFSET) = 0;
        // enable timer1 and timer2
        *(volatile uint32_t*)(VIC1_BASE + VIC_INT_ENABLE_OFFSET) =
            (1 << 4) | (1 << 5);

        *(volatile uint32_t*)(TIMER1_BASE + CRTL_OFFSET) = 0;
        *(volatile uint32_t*)(TIMER1_BASE + LDR_OFFSET) = 3000;
        *(volatile uint32_t*)(TIMER1_BASE + CRTL_OFFSET) =
            ENABLE_MASK | MODE_MASK;  // periodic

        *(volatile uint32_t*)(TIMER2_BASE + CRTL_OFFSET) = 0;
        *(volatile uint32_t*)(TIMER2_BASE + LDR_OFFSET) = 2000;
        *(volatile uint32_t*)(TIMER2_BASE + CRTL_OFFSET) =
            ENABLE_MASK | MODE_MASK;  // periodic

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
        char* actual_user_stacks_end =
            &__USER_STACKS_START__ + (MAX_SCHEDULED_TASKS * USER_STACK_SIZE);
        if (actual_user_stacks_end > &__USER_STACKS_END__) {
            kpanic(
                "actual_user_stacks_end (%p) > &__USER_STACKS_END__ (%p). We "
                "should change MAX_SCHEDULED_TASKS (currently %d) and/or "
                "USER_STACK_SIZE (currently 0x%x).",
                actual_user_stacks_end, &__USER_STACKS_END__,
                MAX_SCHEDULED_TASKS, USER_STACK_SIZE);
        }
#pragma GCC diagnostic pop

        int tid = Create(4, (void*)user_main);
        if (tid < 0) kpanic("could not create tasks (error code %d)", tid);
    }
};  // class Kernel

extern void FirstUserTask();

static Kernel kern;

extern "C" void handle_syscall(uint32_t no, void* user_sp) {
    kern.handle_syscall(no, user_sp);
}

extern "C" void handle_interrupt(void* user_sp) {
    kern.handle_interrupt(user_sp);
}

int kmain() {
    kprintf("Hello from the choochoos kernel!");

    kern.initialize(FirstUserTask);

    while (true) {
        std::optional<Tid> next_task = kern.schedule();
        if (!next_task.has_value()) break;
        kern.activate(next_task.value());
    }

    kprintf("Goodbye from choochoos kernel!");

    return 0;
}
