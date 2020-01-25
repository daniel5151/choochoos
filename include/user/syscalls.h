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

#ifdef __cplusplus
}
#endif
