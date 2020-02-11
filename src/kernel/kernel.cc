#include <cstring>  // memcpy
#include <optional>

#include "common/bwio.h"
#include "common/ts7200.h"
#include "common/vt_escapes.h"
#include "kernel/asm.h"

#include "kernel/kernel.h"
#include "kernel/user_stack.h"

#include "kernel/tasks/nameserver.h"

// CONTRACT: userland must supply a FirstUserTask function
extern void FirstUserTask();

namespace kernel {

static std::optional<size_t> current_interrupt() {
    uint32_t vic1_bits =
        *((volatile uint32_t*)VIC1_BASE + VIC_IRQ_STATUS_OFFSET);

    for (size_t i = 0; i < 32; i++) {
        if (vic1_bits & (1 << i)) {
            return i;
        }
    }

    uint32_t vic2_bits =
        *((volatile uint32_t*)VIC2_BASE + VIC_IRQ_STATUS_OFFSET);

    for (size_t i = 0; i < 32; i++) {
        if (vic2_bits & (1 << i)) {
            return 32 + i;
        }
    }

    return std::nullopt;
}

static const Tid IDLE_TASK_TID = Tid(MAX_SCHEDULED_TASKS - 1);

std::optional<Tid> Kernel::next_tid() {
    for (size_t tid = 0; tid < MAX_SCHEDULED_TASKS; tid++) {
        if (!tasks[tid].has_value()) return Tid(tid);
    }
    return std::nullopt;
}

Kernel::Kernel()
    : tasks{std::nullopt},
      event_queue{},
      ready_queue{},
      current_task{UINT32_MAX} {}

void Kernel::handle_syscall(uint32_t no, void* user_sp) {
    kassert(tasks[current_task].has_value());

    tasks[current_task].value().sp = user_sp;

    UserStack* user_stack = (UserStack*)user_sp;
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
        default:
            kpanic("invalid syscall %lu", no);
    }
    if (ret.has_value()) {
        TaskDescriptor::write_syscall_return_value(tasks[current_task].value(),
                                                   ret.value());
    }
}

void Kernel::handle_interrupt() {
    auto no_opt = current_interrupt();
    if (!no_opt.has_value())
        kpanic("current_interrupt(): no interrupts are set");
    uint32_t no = no_opt.value();

    kdebug("handle_interrupt: no=%lu", no);

    kassert(tasks[current_task].has_value());
    kassert(no < 64);

    int ret;

    // assert interrupt, get return value
    switch (no) {
        case 4:
            *(volatile uint32_t*)(TIMER1_BASE + CLR_OFFSET) = 1;
            ret = 0;
            break;
        case 5:
            *(volatile uint32_t*)(TIMER2_BASE + CLR_OFFSET) = 1;
            ret = 0;
            break;
        case 51:
            *(volatile uint32_t*)(TIMER3_BASE + CLR_OFFSET) = 1;
            ret = 0;
            break;
        default:
            kpanic("unexpected interrupt number (%lu)", no);
    }

    // if nobody is waiting for the interrupt, drop it
    std::optional<Tid> blocked_tid_opt = event_queue.take(no);
    if (!blocked_tid_opt.has_value()) return;

    Tid blocked_tid = blocked_tid_opt.value();
    kassert(tasks[blocked_tid].has_value());
    TaskDescriptor& blocked_task = tasks[blocked_tid].value();
    kassert(blocked_task.state.tag = TaskState::EVENT_WAIT);
    blocked_task.state = {.tag = TaskState::READY, .ready = {}};
    TaskDescriptor::write_syscall_return_value(blocked_task, ret);

    if (ready_queue.push(blocked_tid, blocked_task.priority) ==
        PriorityQueueErr::FULL) {
        kpanic("ready queue full");
    }
}

std::optional<Tid> Kernel::schedule() { return ready_queue.pop(); }

void Kernel::activate(Tid tid) {
    kdebug("activating tid %u", (size_t)tid);
    current_task = tid;
    if (!tasks[tid].has_value()) return;
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
            break;
    }
}  // namespace kernel

void Kernel::initialize() {
    *((uint32_t*)0x028) = (uint32_t)((void*)_swi_handler);
    *((uint32_t*)0x038) = (uint32_t)((void*)_irq_handler);

    // unlock system controller sw lock
    *(volatile uint32_t*)(SYSCON_SWLOCK) = 0xaa;
    // enable halt/standby magic addresses
    uint32_t device_cfg = *(volatile uint32_t*)(SYSCON_DEVICECFG);
    *(volatile uint32_t*)(SYSCON_DEVICECFG) = device_cfg | 1;
    // system controller re-locks itself

    // enable protection (prevents user tasks from poking VIC registers)
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_PROTECTION_OFFSET) = 1;
    // all IRQs
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_SELECT_OFFSET) = 0;
    // enable timer2 interrupts
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_ENABLE_OFFSET) = (1 << 5);

    // initialize timer 3 to count down from UINT32_MAX at 508KHz
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER3_BASE + LDR_OFFSET) = UINT32_MAX;
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) =
        ENABLE_MASK | CLKSEL_MASK;

    // initialize timer2 to fire interrupts every 10 ms
    *(volatile uint32_t*)(TIMER2_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER2_BASE + LDR_OFFSET) = 20;
    *(volatile uint32_t*)(TIMER2_BASE + CRTL_OFFSET) = ENABLE_MASK | MODE_MASK;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    char* actual_user_stacks_end =
        &__USER_STACKS_START__ + (MAX_SCHEDULED_TASKS * USER_STACK_SIZE);
    if (actual_user_stacks_end > &__USER_STACKS_END__) {
        kpanic(
            "actual_user_stacks_end (%p) > &__USER_STACKS_END__ (%p). We "
            "should change MAX_SCHEDULED_TASKS (currently %d) and/or "
            "USER_STACK_SIZE (currently 0x%x).",
            actual_user_stacks_end, &__USER_STACKS_END__, MAX_SCHEDULED_TASKS,
            USER_STACK_SIZE);
    }
#pragma GCC diagnostic pop

    // Spawn the name server with a direct call to _create_task, which
    // allows negative priorities and a forced tid.
    _create_task(0, (void*)NameServer::Task, Tid(NameServer::TID));
    Create(0, (void*)FirstUserTask);
}

void Kernel::shutdown() {
    // clear the timers
    *(volatile uint32_t*)(TIMER1_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER2_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) = 0;

    // disable all interrupts
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_ENABLE_OFFSET) = 0;
    *(volatile uint32_t*)(VIC2_BASE + VIC_INT_ENABLE_OFFSET) = 0;
}

size_t Kernel::num_event_blocked_tasks() const {
    return event_queue.num_present();
}
}  // namespace kernel

static kernel::Kernel kern;

extern "C" void handle_syscall(uint32_t no, void* user_sp) {
    kern.handle_syscall(no, user_sp);
}

extern "C" void handle_interrupt() { kern.handle_interrupt(); }

const volatile uint32_t* TIMER3_VAL =
    (volatile uint32_t*)(TIMER3_BASE + VAL_OFFSET);

int kmain() {
    kprintf("Hello from the choochoos kernel!");

    kern.initialize();

    uint32_t idle_time = 0;
    uint32_t idle_timer;

    while (true) {
        std::optional<kernel::Tid> next_task = kern.schedule();
        if (next_task.has_value()) {
            const kernel::Tid tid = next_task.value();
            kern.activate(tid);
        } else {
            if (kern.num_event_blocked_tasks() == 0) break;

            // idle task time!

            // This is pretty neat.
            //
            // We request the system controller to put us into a halt state,
            // and to wake up up when an IRQ happens. All good right? But
            // hey, we're in the kernel, and aren't currently accepting
            // IRQs, so this shouldn't work, right?
            //
            // Wrong!
            //
            // The system controller will freeze the PC at this line, and
            // once an IRQ fires, it simply resumes the PC, _without_
            // jumping to the IRQ handler! Instead, we manually invoke the
            // kernel's interrupt handler, which will unblock any blocked
            // tasks.
            idle_timer = *TIMER3_VAL;
            *(volatile uint32_t*)(SYSCON_HALT);
            idle_time += idle_timer - *TIMER3_VAL;

            kern.handle_interrupt();

#ifndef NO_IDLE_MEASUREMENTS
            bwprintf(COM2,
                     VT_SAVE VT_ROWCOL(1, 60) "[Idle Time %lu%%]" VT_RESTORE,
                     100 * idle_time / (UINT32_MAX - *TIMER3_VAL));
#endif
        }
    }

    kern.shutdown();
    kprintf("Goodbye from choochoos kernel!");

    return 0;
}
