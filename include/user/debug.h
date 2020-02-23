#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "common/bwio.h"
#include "common/vt_escapes.h"
#include "user/syscalls.h"

#ifdef __cplusplus
#define SHUTDOWN ::Shutdown
#else
#define SHUTDOWN Shutdown
#endif

#undef assert

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr)                                                          \
    do {                                                                      \
        if (!(expr)) {                                                        \
            int my_tid = MyTid();                                             \
            bwprintf(COM2,                                                    \
                     VT_RED                                                   \
                     "[assert:%s:%d tid=%d] Assertion failed: (%s), exiting " \
                     "task\r\n" VT_NOFMT,                                     \
                     __FILE__, __LINE__, my_tid, #expr);                      \
            SHUTDOWN();                                                       \
        }                                                                     \
    } while (false)
#endif

/// Print a big scary error message and calls Shutdown()
#define panic(fmt, ...)                                                    \
    do {                                                                   \
        int my_tid = MyTid();                                              \
        bwprintf(COM2, VT_RED "[panic:%s:%d tid=%d] " VT_NOFMT fmt "\r\n", \
                 __FILE__, __LINE__, my_tid, ##__VA_ARGS__);               \
        SHUTDOWN();                                                        \
    } while (false)

#ifdef RELEASE_MODE
#define debug(fmt, ...)
#else
#define debug(fmt, ...)                                                 \
    bwprintf(COM2, VT_CYAN "[debug:%s:%d tid=%d] " VT_NOFMT fmt "\r\n", \
             __FILE__, __LINE__, MyTid(), ##__VA_ARGS__);
#endif

// Omit __FILE__ and __LINE__ from release builds for easier regression testing
#ifdef RELEASE_MODE
#define log(fmt, ...)                                                    \
    do {                                                                 \
        int my_tid = MyTid();                                            \
        bwprintf(COM2, VT_GREEN "[tid=%d] " VT_NOFMT fmt "\r\n", my_tid, \
                 ##__VA_ARGS__);                                         \
    } while (false)

#else
#define log(fmt, ...)                                                      \
    do {                                                                   \
        int my_tid = MyTid();                                              \
        bwprintf(COM2, VT_GREEN "[log:%s:%d tid=%d] " VT_NOFMT fmt "\r\n", \
                 __FILE__, __LINE__, my_tid, ##__VA_ARGS__);               \
    } while (false)

#endif
