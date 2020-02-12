#include "common/bwio.h"
#include "common/ts7200.h"
#include "kernel/kernel.h"

namespace kernel::driver {

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

void handle_interrupt() {
    auto no_opt = current_interrupt();
    if (!no_opt.has_value())
        kpanic("current_interrupt(): no interrupts are set");
    uint32_t no = no_opt.value();

    kdebug("handle_interrupt: no=%lu", no);

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
        case 54: {
            volatile uint32_t* const UART2_CTLR =
                (volatile uint32_t*)(UART2_BASE + UART_CTLR_OFFSET);
            volatile uint32_t* const UART2_INTR =
                (volatile uint32_t*)(UART2_BASE + UART_INTR_OFFSET);

            ret = *UART2_INTR;

            UARTIntIDIntClr u2_int_id = {.raw = (uint32_t)ret};
            UARTCtrl u2_ctlr = {.raw = *UART2_CTLR};

            kassert(!u2_int_id._.modem);  // we don't use the modem
            if (u2_int_id._.rx) u2_ctlr._.enable_int_rx = false;
            if (u2_int_id._.tx) u2_ctlr._.enable_int_tx = false;
            if (u2_int_id._.rx_timeout) u2_ctlr._.enable_int_rx_timeout = false;

            *UART2_CTLR = u2_ctlr.raw;
        } break;
        default:
            kpanic("unexpected interrupt number (%lu)", no);
    }

    kdebug("irq volatile data: 0x%08lx", (uint32_t)ret);

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

}  // namespace kernel::driver

extern "C" void handle_interrupt() { kernel::driver::handle_interrupt(); }
