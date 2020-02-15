#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"

volatile uint32_t* const UART2_CTLR =
    (volatile uint32_t*)(UART2_BASE + UART_CTLR_OFFSET);
volatile uint32_t* const UART2_FLAG =
    (volatile uint32_t*)(UART2_BASE + UART_FLAG_OFFSET);
volatile uint32_t* const UART2_DATA =
    (volatile uint32_t*)(UART2_BASE + UART_DATA_OFFSET);

#include <cstring>
const char* msg =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum." ENDL;

void TXPlayground() {
    bwsetfifo(COM2, true);

    size_t written = 0;
    size_t len = strlen(msg);

    while (true) {
        UARTCtrl u2_ctlr = {.raw = *UART2_CTLR};
        u2_ctlr._.enable_int_tx = true;
        *UART2_CTLR = u2_ctlr.raw;

        AwaitEvent(54);
        //
        //
        for (; written < len && !(*UART2_FLAG & TXFF_MASK); written++) {
            *UART2_DATA = (uint32_t)msg[written];
        }

        if (written >= len) return;
    }
}

void RXPlayground() {
    bwsetfifo(COM2, true);

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

void FirstUserTask() {
    // Only run one of these - AwaitEvent() can't be called by two simultaneous
    // tasks.

    Create(0, RXPlayground);
    // Create(0, TXPlayground);
}
