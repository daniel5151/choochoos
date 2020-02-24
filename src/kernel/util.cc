#include "kernel/kernel.h"

// #include <cctype>
#include <cstdarg>
#include <cstdio>

#include "common/bwio.h"

extern "C" {
void _exit(int status) __attribute__((noreturn));
}

void kexit(int status) {
    kernel::driver::shutdown();
    _exit(status);
}

// TODO: implement a backtrace / crash-dump mechanism?
void kpanic(const char* fmt, ...) {
    va_list va;

    va_start(va, fmt);
    bwputstr(COM2, "Kernel panic! ");
    vbwprintf(COM2, fmt, va);
    bwputstr(COM2, "\r\n");
    va_end(va);

    bwflush(COM2);

    kexit(1);
}

// TODO: cache kprintf's in-memory,  only flushing them when requested/required
// e.g: via a user command / when a kpanic fires
void kprintf(const char* fmt, ...) {
    va_list va;

    va_start(va, fmt);
    vbwprintf(COM2, fmt, va);
    bwputstr(COM2, "\r\n");
    va_end(va);
}
