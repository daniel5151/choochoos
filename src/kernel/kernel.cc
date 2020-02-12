#include "kernel/kernel.h"

#include <cstring>  // memcpy
#include <optional>

#include "common/bwio.h"
#include "common/ts7200.h"
#include "common/vt_escapes.h"
#include "kernel/asm.h"
#include "kernel/tasks/nameserver.h"

// CONTRACT: userland must supply a FirstUserTask function
extern void FirstUserTask();

namespace kernel::driver {

static const Tid IDLE_TASK_TID = Tid(MAX_SCHEDULED_TASKS - 1);

std::optional<Tid> schedule() { return ready_queue.pop(); }

void activate(Tid tid) {
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

void initialize() {
    *((volatile uint32_t*)0x028) = (uint32_t)((void*)_swi_handler);
    *((volatile uint32_t*)0x038) = (uint32_t)((void*)_irq_handler);

    // unlock system controller sw lock
    *(volatile uint32_t*)(SYSCON_SWLOCK) = 0xaa;
    // enable halt/standby magic addresses
    uint32_t device_cfg = *(volatile uint32_t*)(SYSCON_DEVICECFG);
    *(volatile uint32_t*)(SYSCON_DEVICECFG) = device_cfg | 1;
    // system controller re-locks itself

    // enable protection (prevents user tasks from poking VIC registers)
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_PROTECTION_OFFSET) = 1;
    *(volatile uint32_t*)(VIC2_BASE + VIC_INT_PROTECTION_OFFSET) = 1;
    // all interrupts are handled as IRQs
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_SELECT_OFFSET) = 0;
    *(volatile uint32_t*)(VIC2_BASE + VIC_INT_SELECT_OFFSET) = 0;
    // enable timer2 interrupts
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_ENABLE_OFFSET) = (1 << 5);
    // enable uart2 combined interrupt
    *(volatile uint32_t*)(VIC2_BASE + VIC_INT_ENABLE_OFFSET) = (1 << (54 - 32));

    // initialize timer 3 to count down from UINT32_MAX at 508KHz
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER3_BASE + LDR_OFFSET) = UINT32_MAX;
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) =
        ENABLE_MASK | CLKSEL_MASK;

#ifndef NENABLE_CACHES
    _enable_caches();
#endif

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
    helpers::create_task(0, (void*)NameServer::Task, Tid(NameServer::TID));
    handlers::Create(0, (void*)FirstUserTask);
}

void shutdown() {
    // clear the timers
    *(volatile uint32_t*)(TIMER1_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER2_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) = 0;

    // disable all interrupts
    *(volatile uint32_t*)(VIC1_BASE + VIC_INT_ENABLE_OFFSET) = 0;
    *(volatile uint32_t*)(VIC2_BASE + VIC_INT_ENABLE_OFFSET) = 0;
}

size_t num_event_blocked_tasks() { return event_queue.num_present(); }
}  // namespace kernel::driver

namespace kernel {

std::optional<TaskDescriptor> tasks[MAX_SCHEDULED_TASKS];
OptArray<Tid, 64> event_queue;
PriorityQueue<Tid, MAX_SCHEDULED_TASKS> ready_queue;
Tid current_task = -1;

int run() {
    kprintf("Hello from the choochoos kernel!");

    driver::initialize();

    // initialize timer3 to count down from UINT32_MAX at 508KHz
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) = 0;
    *(volatile uint32_t*)(TIMER3_BASE + LDR_OFFSET) = UINT32_MAX;
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) =
        ENABLE_MASK | CLKSEL_MASK;

    uint32_t idle_time = 0;
    uint32_t idle_timer;

    while (true) {
        std::optional<kernel::Tid> next_task = driver::schedule();
        if (next_task.has_value()) {
            const kernel::Tid tid = next_task.value();
            driver::activate(tid);
        } else {
            if (driver::num_event_blocked_tasks() == 0) break;

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

            static const volatile uint32_t* TIMER3_VAL =
                (volatile uint32_t*)(TIMER3_BASE + VAL_OFFSET);

            idle_timer = *TIMER3_VAL;
            *(volatile uint32_t*)(SYSCON_HALT);
            idle_time += idle_timer - *TIMER3_VAL;

            driver::handle_interrupt();

#ifndef NO_IDLE_MEASUREMENTS
            bwprintf(COM2,
                     VT_SAVE VT_ROWCOL(1, 60) "[Idle Time %lu%%]" VT_RESTORE,
                     100 * idle_time / (UINT32_MAX - *TIMER3_VAL));
#endif
        }
    }

    driver::shutdown();
    kprintf("Goodbye from choochoos kernel!");

    return 0;
}
}  // namespace kernel
