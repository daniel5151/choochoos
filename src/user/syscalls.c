#include "user/syscalls.h"

#include <stdbool.h>

#include "user/debug.h"
#include "user/syscalls.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

// Raw Syscall signatures

void __Shutdown(void) __attribute__((noreturn));
void __Perf(struct perf_t* perf);

int __AwaitEvent(int eventid);
int __Reply(int tid, const char* reply, int rplen);
int __Receive(int* tid, char* msg, int msglen);
int __Send(int tid, const char* msg, int msglen, char* reply, int rplen);
int __Create(int priority, void (*function)());
int __MyTid(void);
int __MyParentTid(void);
void __Exit(void) __attribute__((noreturn));
void __Yield(void);

// Wrapper methods around raw syscalls

void Shutdown(void) { __Shutdown(); }
void Perf(struct perf_t* perf) { __Perf(perf); }

int AwaitEvent(int eventid) { return __AwaitEvent(eventid); }
int Reply(int tid, const char* reply, int rplen) {
    return __Reply(tid, reply, rplen);
}
int Receive(int* tid, char* msg, int msglen) {
    return __Receive(tid, msg, msglen);
}
int Send(int tid, const char* msg, int msglen, char* reply, int rplen) {
    int ret = __Send(tid, msg, msglen, reply, rplen);
    assert(ret >= -2);
    return ret;
}
int Create(int priority, void (*function)()) {
    return __Create(priority, function);
}
int MyTid(void) { return __MyTid(); }
int MyParentTid(void) { return __MyParentTid(); }
void Exit(void) { __Exit(); }
void Yield(void) { __Yield(); }
