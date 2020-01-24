#pragma once

#include "vt_escapes.h"

#ifdef __cplusplus
extern "C" {
#endif

int Create(int priority, void (*function)());
int MyTid();
int MyParentTid();
void Exit();
void Yield();
int Send(int tid, const char* msg, int msglen, char* reply, int rplen);
int Receive(int* tid, char* msg, int msglen);
int Reply(int tid, const char* reply, int rplen);

#undef assert
#define assert(expr)                                                \
    {                                                               \
        if (!(expr)) {                                              \
            bwprintf(COM2,                                          \
                     VT_RED "[assert:" __FILE__                     \
                            ":%d tid=%d] Assertion failed: (" #expr \
                            ")\r\n" VT_NOFMT,                       \
                     __LINE__, MyTid());                            \
            Exit();                                                 \
        }                                                           \
    }

#ifdef __cplusplus
}
#endif
