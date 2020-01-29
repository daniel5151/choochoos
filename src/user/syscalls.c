#include "user/syscalls.h"

#include <stdbool.h>

#include "user/dbg.h"
#include "user/syscalls.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

// Raw Syscall signatures

int __Reply(int tid, const char* reply, int rplen);
int __Receive(int* tid, char* msg, int msglen);
int __Send(int tid, const char* msg, int msglen, char* reply, int rplen);
int __Create(int priority, void (*function)());
int __MyTid();
int __MyParentTid();
void __Exit() __attribute__((noreturn));
void __Yield();

// Wrapper methods around raw syscalls

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
int MyTid() { return __MyTid(); }
int MyParentTid() { return __MyParentTid(); }
void Exit() { __Exit(); }
void Yield() { return __Yield(); }
