#include <cstddef>

#include "common/bwio.h"
#include "common/priority_queue.h"
#include "common/ts7200.h"
#include "kernel/asm.h"
#include "kernel/kernel.h"

extern int kmain();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    bwsetfifo(COM2, false);

    // XXX: this setup should be in the clock server!
    // set up timer3
    *(volatile uint32_t*)(TIMER3_BASE + LDR_OFFSET) = 0xfffff;
    *(volatile uint32_t*)(TIMER3_BASE + CRTL_OFFSET) = ENABLE_MASK | MODE_MASK | CLKSEL_MASK;  // periodic + 508 kHz
    // enable timer3 interrupts
    *(volatile uint32_t*)(VIC2_BASE + VIC_INT_ENABLE_OFFSET) = 1 << (51 - 32);

#ifdef ENABLE_CACHES
    _enable_caches();
#endif

    return kmain();
}
