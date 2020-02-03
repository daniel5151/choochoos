#include "kernel/kernel.h"

#define STUB(fn) \
    void fn() { kpanic("newlib tried to call " #fn); }

STUB(_exit)
STUB(_sbrk)
STUB(_kill)
STUB(_getpid)
STUB(_write)
STUB(_close)
STUB(_fstat)
STUB(_isatty)
STUB(_lseek)
STUB(_read)
