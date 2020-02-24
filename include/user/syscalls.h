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
void Shutdown(void) __attribute__((noreturn));
void Perf(struct perf_t* perf);

// Base Syscalls
int Create(int priority, void (*function)());
int MyTid(void);
int MyParentTid(void);
void Exit(void) __attribute__((noreturn));
void Yield(void);
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
