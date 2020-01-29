#pragma once

#include <stdio.h>

#include "common/bwio.h"
#include "common/vt_escapes.h"

#define LOG_BUFFER_SIZE 128

#define ENDL "\r\n"

#undef assert
#define assert(expr)                                                \
    do {                                                            \
        if (!(expr)) {                                              \
            bwprintf(COM2,                                          \
                     VT_RED "[assert:" __FILE__                     \
                            ":%d tid=%d] Assertion failed: (" #expr \
                            "), exiting task\r\n" VT_NOFMT,         \
                     __LINE__, MyTid());                            \
            Exit();                                                 \
        }                                                           \
    } while (false)

#if defined(DISABLE_KDEBUG_PRINTS) || defined(RELEASE_MODE)
#define debug(fmt, args...)
#else
#define debug(fmt, args...)                                             \
    do {                                                                \
        char buf[LOG_BUFFER_SIZE];                                      \
        snprintf(buf, LOG_BUFFER_SIZE,                                  \
                 VT_CYAN "[debug:" __FILE__ ":%d tid=%d] " VT_NOFMT fmt \
                         "\r\n",                                        \
                 __LINE__, MyTid(), ##args);                            \
        bwputstr(COM2, buf);                                            \
    } while (false)
#endif

#if defined(DISABLE_KDEBUG_PRINTS) || defined(RELEASE_MODE)
#define log(fmt, args...)                                                    \
    do {                                                                     \
        char buf[LOG_BUFFER_SIZE];                                           \
        snprintf(buf, LOG_BUFFER_SIZE,                                       \
                 VT_GREEN "[tid=%d] " VT_NOFMT fmt "\r\n", MyTid(), ##args); \
        bwputstr(COM2, buf);                                                 \
    } while (false)
#else
#define log(fmt, args...)                                                      \
    do {                                                                       \
        char buf[LOG_BUFFER_SIZE];                                             \
        snprintf(buf, LOG_BUFFER_SIZE,                                         \
                 VT_GREEN "[log:" __FILE__ ":%d tid=%d] " VT_NOFMT fmt "\r\n", \
                 __LINE__, MyTid(), ##args);                                   \
        bwputstr(COM2, buf);                                                   \
    } while (false)
#endif

#undef panic
#define panic(fmt, args...)                                                    \
    do {                                                                       \
        bwprintf(COM2,                                                         \
                 VT_RED "[panic:" __FILE__ ":%d tid=%d] " VT_NOFMT fmt "\r\n", \
                 __LINE__, MyTid(), ##args);                                   \
        Exit();                                                                \
    } while (false)

// the (null-terminated) output has been completely written if and only if the
// returned value is nonnegative and less than buf_size
#define PRINTF_BUF_SIZE 1024
#define printf(fmt, args...)                                                   \
    do {                                                                       \
        char __printf_buf[PRINTF_BUF_SIZE];                                    \
        int __printf_n = snprintf(__printf_buf, PRINTF_BUF_SIZE, fmt, ##args); \
        if (__printf_n < 0 || __printf_n >= PRINTF_BUF_SIZE) {                 \
            panic("printf: short write (n=%d, buf_size=%d)  [" __FILE__        \
                  ":%d]",                                                      \
                  __printf_n, PRINTF_BUF_SIZE, __LINE__);                      \
        }                                                                      \
        bwputstr(COM2, __printf_buf);                                          \
    } while (false)
