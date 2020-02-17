#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"

volatile uint32_t* const UART2_CTLR =
    (volatile uint32_t*)(UART2_BASE + UART_CTLR_OFFSET);
const volatile uint32_t* const UART2_FLAG =
    (volatile uint32_t*)(UART2_BASE + UART_FLAG_OFFSET);
volatile uint32_t* const UART2_DATA =
    (volatile uint32_t*)(UART2_BASE + UART_DATA_OFFSET);

volatile uint32_t* const UART1_CTLR =
    (volatile uint32_t*)(UART1_BASE + UART_CTLR_OFFSET);
const volatile uint32_t* const UART1_FLAG =
    (volatile uint32_t*)(UART1_BASE + UART_FLAG_OFFSET);
volatile uint32_t* const UART1_DATA =
    (volatile uint32_t*)(UART1_BASE + UART_DATA_OFFSET);

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
        for (; written < len && !(*UART2_FLAG & TXFF_MASK); written++) {
            *UART2_DATA = (uint32_t)msg[written];
        }

        if (written >= len) return;
    }
}

void TrainPlayground() {
    // set up uart 1
    {
        volatile int* mid = (volatile int*)(UART1_BASE + UART_LCRM_OFFSET);
        volatile int* low = (volatile int*)(UART1_BASE + UART_LCRL_OFFSET);
        volatile int* high = (volatile int*)(UART1_BASE + UART_LCRH_OFFSET);

        *mid = 0x0;
        *low = 0xbf;

        int buf = *high;
        buf = (buf | STP2_MASK) & ~FEN_MASK;
        *high = buf;
    }

    size_t written = 0;
    size_t len = strlen(msg);

    while (true) {
        size_t n = written;
        for (; written < len && !(*UART1_FLAG & TXFF_MASK) &&
               (*UART1_FLAG & CTS_MASK);
             written++) {
            *UART1_DATA = (uint32_t)msg[written];
        }
        n = written - n;
        bwprintf(COM2, "wrote %d bytes to COM1" ENDL, n);

        if (written >= len) return;

        UARTCtrl u1_ctlr = {.raw = *UART1_CTLR};
        u1_ctlr._.enable_int_tx = true;
        // TODO I not entirely sure if this is the interrupt that tells us when
        // the CTS flag is flipped. We need to test this on real hardware.
        u1_ctlr._.enable_int_modem = true;
        *UART1_CTLR = u1_ctlr.raw;
        AwaitEvent(52);
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

    // Create(0, RXPlayground);
    //    Create(0, TXPlayground);
    Create(0, TrainPlayground);
}
