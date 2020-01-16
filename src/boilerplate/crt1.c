#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "bwio.h"

typedef void (*ctr_fn)();

// Defined by the linker script
extern char __BSS_START__, __BSS_END__;
extern ctr_fn __INIT_ARRAY_START__, __INIT_ARRAY_END__;

static void* redboot_return_addr;

void kexit(int status) {
    __asm__ volatile("mov r0, %0" ::"r"(status));
    __asm__ volatile("mov pc, %0" ::"r"(redboot_return_addr));
}

void kpanic(const char* fmt, ...) {
    char buf[1000];
    va_list va;

    va_start(va, fmt);
    vsnprintf(buf, 1000, fmt, va);
    va_end(va);

    bwprintf(COM2, "Kernel panic: %s\r\n", buf);

    kexit(1);
}

// forward-declaration of main
int main(int argc, char* argv[]);

int _start() {
    // Saves the LR into redboot_return_addr, allowing kexit() to jump straight
    // back to RedBoot. This line must be executed before any functions are
    // called to ensure that the LR isn't modified.
    __asm__ volatile("mov %0, lr" : "=r"(redboot_return_addr));

    // Zero out .bss
    memset(&__BSS_START__, 0, (size_t)(&__BSS_END__ - &__BSS_START__));

    // Run C++ global constructors
    for (ctr_fn* ctr = &__INIT_ARRAY_START__; ctr < &__INIT_ARRAY_END__; ctr++)
        (*ctr)();

    // Invoke main
    // TODO? see if redboot supports passing parameters?
    int status = main(0, 0);

    // TODO? global dtors?

    return status;
}
