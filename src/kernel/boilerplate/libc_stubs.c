#include "kernel/helpers.h"
#include "user/debug.h"

// unified panic handler since user and kernel code could both accidentally call
// libc functions.
inline static void panic_(const char* fn) {
    int cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    if ((cpsr & 0x1f) == 0x10) {
        // user mode
        panic("newlib tried to call %s", fn);
    } else {
        // kernel mode
        kpanic("newlib tried to call %s", fn);
    }
}

#define STUB(fn) \
    void fn() { panic_(#fn); }

// STUB(_exit) // defined in crt0, returns to redboot
STUB(_sbrk)
STUB(_kill)
STUB(_getpid)
STUB(_write)
STUB(_close)
STUB(_fstat)
STUB(_isatty)
STUB(_lseek)
STUB(_read)

void _putchar(char c) {
    (void)c;
    kpanic("tried to use raw printf method (instead of a fctprintf wrapper)");
}
