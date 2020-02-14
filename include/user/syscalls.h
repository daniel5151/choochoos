#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct perf_t {
    uint32_t idle_time_pct;
    // TODO: add more juicy data
};

// Extra Syscalls
void Perf(struct perf_t* perf);

// Base Syscalls
int Create(int priority, void (*function)());
int MyTid();
int MyParentTid();
void Exit() __attribute__((noreturn));
void Yield();
int Send(int tid, const char* msg, int msglen, char* reply, int rplen);
int Receive(int* tid, char* msg, int msglen);
int Reply(int tid, const char* reply, int rplen);
int AwaitEvent(int eventid);

// "Syscalls"
int WhoIs(const char* name);
int RegisterAs(const char* name);

#ifdef __cplusplus
}
#endif
