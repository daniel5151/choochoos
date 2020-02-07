#pragma once

#include <stdio.h>

#include "common/bwio.h"
#include "common/vt_escapes.h"
#include "user/syscalls.h"

#define LOG_BUFFER_SIZE 128

#define ENDL "\r\n"

#undef assert

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr)                                                          \
    do {                                                                      \
        if (!(expr)) {                                                        \
            bwprintf(COM2, VT_RED                                             \
                     "[assert:%s:%d tid=%d] Assertion failed: (%s), exiting " \
                     "task\r\n" VT_NOFMT,                                     \
                     __FILE__, __LINE__, MyTid(), #expr);                     \
            Exit();                                                           \
        }                                                                     \
    } while (false)
#endif

/// Print a big scary error message and calls Exit()
// TODO: make a dedicated syscall for panics
#define panic(fmt, ...)                                                    \
    do {                                                                   \
        bwprintf(COM2, VT_RED "[panic:%s:%d tid=%d] " VT_NOFMT fmt "\r\n", \
                 __FILE__, __LINE__, MyTid(), ##__VA_ARGS__);              \
        Exit();                                                            \
    } while (false)


#ifdef RELEASE_MODE
#define debug(fmt, ...)
#else
#define debug(fmt, ...)                                               \
    do {                                                              \
        char buf[LOG_BUFFER_SIZE];                                    \
        snprintf(buf, LOG_BUFFER_SIZE,                                \
                 VT_CYAN "[debug:%s:%d tid=%d] " VT_NOFMT fmt "\r\n", \
                 __FILE__, __LINE__, MyTid(), ##__VA_ARGS__);         \
        bwputstr(COM2, buf);                                          \
    } while (false)
#endif

// Omit __FILE__ and __LINE__ from release builds for easier regression testing
#ifdef RELEASE_MODE
#define log(fmt, ...)                                               \
    do {                                                            \
        char buf[LOG_BUFFER_SIZE];                                  \
        snprintf(buf, LOG_BUFFER_SIZE,                              \
                 VT_GREEN "[tid=%d] " VT_NOFMT fmt "\r\n", MyTid(), \
                 ##__VA_ARGS__);                                    \
        bwputstr(COM2, buf);                                        \
    } while (false)
#else
#define log(fmt, ...)                                                          \
    do {                                                                       \
        char buf[LOG_BUFFER_SIZE];                                             \
        snprintf(buf, LOG_BUFFER_SIZE,                                         \
                 VT_GREEN "[log:%s:%d tid=%d] " VT_NOFMT fmt "\r\n", __FILE__, \
                 __LINE__, MyTid(), ##__VA_ARGS__);                            \
        bwputstr(COM2, buf);                                                   \
    } while (false)
#endif
