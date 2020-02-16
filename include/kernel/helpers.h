#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Immediately terminate the kernel, and exit to redboot
void kexit(int status) __attribute__((noreturn));

/// Print out an error message, and call kexit(1)
void kpanic(const char* fmt, ...) __attribute__((format(printf, 1, 2)))
__attribute__((noreturn));

/// Print to the kernel's log
void kprintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef NDEBUG
#define kassert(expr) ((void)0)
#else
#define kassert(expr)                                                 \
    do {                                                              \
        if (!(expr)) {                                                \
            kpanic("Assertion failed: (%s) [%s:%d]", #expr, __FILE__, \
                   __LINE__);                                         \
        }                                                             \
    } while (false)
#endif

#if defined(RELEASE_MODE) || !defined(KDEBUG)
#define kdebug(...)
#else
#include "common/vt_escapes.h"
#define kdebug(fmt, ...)                                               \
    kprintf(VT_YELLOW "[kdebug:%s:%d tid=%d] " VT_NOFMT fmt, __FILE__, \
            __LINE__, ::kernel::handlers::MyTid(), ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif
