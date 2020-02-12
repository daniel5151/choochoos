#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"

volatile uint32_t* const UART2_CTLR =
    (volatile uint32_t*)(UART2_BASE + UART_CTLR_OFFSET);
volatile uint32_t* const UART2_FLAG =
    (volatile uint32_t*)(UART2_BASE + UART_FLAG_OFFSET);
volatile uint32_t* const UART2_DATA =
    (volatile uint32_t*)(UART2_BASE + UART_DATA_OFFSET);

void FirstUserTask() {
    bwprintf(COM2, "k4" ENDL);

    while (true) {
        UARTCtrl u2_ctlr = {.raw = *UART2_CTLR};
        u2_ctlr._.enable_int_rx = true;
        u2_ctlr._.enable_int_rx_timeout = true;
        *UART2_CTLR = u2_ctlr.raw;

        // wait for an interrupt to come in
        UARTIntIDIntClr int_id = {.raw = (uint32_t)AwaitEvent(54)};
        bwprintf(COM2, "%08lx" ENDL, int_id.raw);

        if (int_id._.rx_timeout) {
            debug("there was a rx timeout");
            assert((*UART2_FLAG & RXFE_MASK) == 0);  // fifo has data

            // drain the fifo
            while ((*UART2_FLAG & RXFE_MASK) == 0)
                bwputc(COM2, (char)*UART2_DATA);
        } else if (int_id._.rx) {
            debug("i have something to rx");
            assert((*UART2_FLAG & RXFE_MASK) == 0);  // fifo has data

            // drain the fifo
            while ((*UART2_FLAG & RXFE_MASK) == 0)
                bwputc(COM2, (char)*UART2_DATA);
        }
    }
}
