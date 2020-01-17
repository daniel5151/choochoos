#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int Create(int priority, void (*function)());
int MyTid();
int MyParentTid();
int Exit();
void Yield();

#ifdef __cplusplus
}
#endif
