#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void kexit(int status) __attribute__((noreturn));
void kpanic(const char* fmt, ...) __attribute__((format(printf, 1, 2)))
__attribute__((noreturn));
void kprintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// #define DISABLE_KDEBUG_PRINTS // TODO: make this a build-time flag

#if defined(DISABLE_KDEBUG_PRINTS) || defined(RELEASE_MODE)
#define kdebug(...)
#else
#include "vt_escapes.h"
#define kdebug(fmt, ...)                                                       \
    kprintf(VT_BLUE "[kdebug:%s:%d tid=%d] " VT_NOFMT fmt, __FILE__, __LINE__, \
            MyTid(), ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif
