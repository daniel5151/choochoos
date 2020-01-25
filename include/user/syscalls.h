#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int Create(int priority, void (*function)());
int MyTid();
int MyParentTid();
void Exit();
void Yield();
int Send(int tid, const char* msg, size_t msglen, char* reply, size_t rplen);
int Receive(int* tid, char* msg, size_t msglen);
int Reply(int tid, const char* reply, size_t rplen);

#ifdef __cplusplus
}
#endif
