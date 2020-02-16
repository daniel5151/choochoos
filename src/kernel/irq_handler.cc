#include "common/bwio.h"
#include "common/ts7200.h"
#include "common/variant_helpers.h"
#include "kernel/kernel.h"

namespace kernel::driver {

static void service_interrupt(size_t no) {
    kdebug("service_interrupt: no=%lu", no);

    kassert(no < 64);

    uint32_t ret;

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
            const volatile uint32_t* const UART2_INTR =
                (volatile uint32_t*)(UART2_BASE + UART_INTR_OFFSET);

            ret = *UART2_INTR;

            UARTIntIDIntClr u2_int_id = {.raw = (uint32_t)ret};
            UARTCtrl u2_ctlr = {.raw = *UART2_CTLR};

            kdebug("kernel: irq 54: u2_int_id=0x%08lx u2_ctlr=0x%08lx" ENDL,
                   u2_int_id.raw, u2_ctlr.raw);

            kassert(!u2_int_id._.modem);  // we don't use the modem
            if (u2_int_id._.rx) u2_ctlr._.enable_int_rx = false;
            if (u2_int_id._.tx) u2_ctlr._.enable_int_tx = false;
            if (u2_int_id._.rx_timeout) u2_ctlr._.enable_int_rx_timeout = false;
            *UART2_CTLR = u2_ctlr.raw;

        } break;
        default:
            kpanic("unexpected interrupt number (%u)", no);
    }

    kdebug("irq volatile data: 0x%08lx", ret);

    // if nobody is waiting for the interrupt, write down the "volatile data"
    std::optional<TidOrVolatileData> blocked_tid_or_volatile_data_opt =
        event_queue.take(no);  // take sets the event_queue[no] to std::nullopt.
    if (!blocked_tid_or_volatile_data_opt.has_value()) {
        kdebug(
            "no tasks are waiting for interrupt no %lu, storing data "
            "0x%lx" ENDL,
            no, ret);
        event_queue.put(VolatileData(ret), no);
        return;
    }

    std::visit(
        overloaded{
            [&](Tid& blocked_tid) {
                // if a task is blocked waiting for the event, wake it up with
                // the volatile data.
                kassert(tasks[blocked_tid].has_value());
                TaskDescriptor& blocked_task = tasks[blocked_tid].value();
                kassert(blocked_task.state.tag = TaskState::EVENT_WAIT);
                blocked_task.state = {.tag = TaskState::READY, .ready = {}};
                TaskDescriptor::write_syscall_return_value(blocked_task, ret);

                if (ready_queue.push(blocked_tid, blocked_task.priority) ==
                    PriorityQueueErr::FULL) {
                    kpanic("ready queue full");
                }
            },
            [&](VolatileData& old_data) {
                // if there was already stored volatile data, replace it.
                (void)old_data;
                kdebug(
                    "replacing volatile data for interrupt %lu, old=0x%x "
                    "new=0x%x",
                    no, old_data.raw(), ret);
                event_queue.put(VolatileData(ret), no);
            }},
        blocked_tid_or_volatile_data_opt.value());
}

// Sometimes multiple interrupts are asserted at the same time. We need to
// handle all of them.
void handle_interrupt() {
    uint32_t vic1_bits =
        *((volatile uint32_t*)VIC1_BASE + VIC_IRQ_STATUS_OFFSET);

    for (size_t i = 0; i < 32; i++) {
        if (vic1_bits & (1 << i)) {
            service_interrupt(i);
        }
    }

    uint32_t vic2_bits =
        *((volatile uint32_t*)VIC2_BASE + VIC_IRQ_STATUS_OFFSET);

    for (size_t i = 0; i < 32; i++) {
        if (vic2_bits & (1 << i)) {
            service_interrupt(32 + i);
        }
    }
}

}  // namespace kernel::driver

extern "C" void handle_interrupt() { kernel::driver::handle_interrupt(); }
