#include "kernel/kernel.h"

// #include <cctype>
#include <cstdarg>
#include <cstdio>

#include "common/bwio.h"

extern "C" {
    void _exit(int status) __attribute__((noreturn));
}

void kexit(int status) {
    _exit(status);
}

// TODO: implement a backtrace / crash-dump mechanism?
void kpanic(const char* fmt, ...) {
    char buf[1024];
    va_list va;

    va_start(va, fmt);
    vsnprintf(buf, 1024, fmt, va);
    va_end(va);

    bwputstr(COM2, "Kernel panic! ");
    bwputstr(COM2, buf);
    bwputstr(COM2, "\r\n");

    kexit(1);
}

// TODO: cache kprintf's in-memory,  only flushing them when requested/required
// e.g: via a user command / when a kpanic fires
void kprintf(const char* fmt, ...) {
    char buf[1024];
    va_list va;

    va_start(va, fmt);
    vsnprintf(buf, 1024, fmt, va);
    va_end(va);

    bwputstr(COM2, buf);
    bwputstr(COM2, "\r\n");
}
