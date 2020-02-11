#include "kernel/helpers.h"

#define STUB(fn) \
    void fn() { kpanic("newlib tried to call " #fn); }

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
