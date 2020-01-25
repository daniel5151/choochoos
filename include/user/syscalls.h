#pragma once

#include <stdio.h>
#include "bwio.h"
#include "vt_escapes.h"

#ifdef __cplusplus
extern "C" {
#endif

int Create(int priority, void (*function)());
int MyTid();
int MyParentTid();
void Exit() __attribute__((noreturn));
void Yield();
int Send(int tid, const char* msg, int msglen, char* reply, int rplen);
int Receive(int* tid, char* msg, int msglen);
int Reply(int tid, const char* reply, int rplen);

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
#define printf(fmt, args...)                                            \
    do {                                                                \
        char buf[PRINTF_BUF_SIZE];                                      \
        int n = snprintf(buf, PRINTF_BUF_SIZE, fmt, ##args);            \
        if (n < 0 || n >= PRINTF_BUF_SIZE) {                            \
            panic("printf: short write (n=%d, buf_size=%d)  [" __FILE__ \
                  ":%d]",                                               \
                  n, PRINTF_BUF_SIZE, __LINE__);                        \
        }                                                               \
        bwputstr(COM2, buf);                                            \
    } while (false)

#ifdef __cplusplus
}
#endif
