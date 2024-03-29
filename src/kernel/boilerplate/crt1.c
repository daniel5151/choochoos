#include <string.h>
#include "user/debug.h"

typedef void (*ctr_fn)();

// Defined by the linker script
extern char __BSS_START__, __BSS_END__;
extern ctr_fn __INIT_ARRAY_START__, __INIT_ARRAY_END__;

static void* redboot_return_addr;

// TODO: _exit should perform cleanup (i.e: call global destructors)
void _exit(int status) {
    int cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    if ((cpsr & 0x1f) == 0x10) {
        // user mode
        panic("newlib tried to call %s", "_exit");
    }
    __asm__ volatile("mov r0, %0" ::"r"(status));
    __asm__ volatile("mov pc, %0" ::"r"(redboot_return_addr));
}

// forward-declaration of main
int main(int argc, char* argv[]);

int _start() {
    // Saves the LR into redboot_return_addr, allowing kexit() to jump straight
    // back to RedBoot. This line must be executed before any functions are
    // called to ensure that the LR isn't modified.
    void* ra;
    __asm__ volatile("mov %0, lr" : "=r"(ra));

    // Zero out .bss
    memset(&__BSS_START__, 0, (size_t)(&__BSS_END__ - &__BSS_START__));

    redboot_return_addr = ra;

    // Run C++ global constructors
    for (ctr_fn* ctr = &__INIT_ARRAY_START__; ctr < &__INIT_ARRAY_END__; ctr++)
        (*ctr)();

    // Invoke main
    // TODO? see if redboot supports passing parameters?
    int status = main(0, 0);

    // TODO? global dtors?

    return status;
}
