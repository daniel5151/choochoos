#include <stddef.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

int Reply(int tid, const char* reply, size_t rplen) {
    __asm__ volatile("swi #7");
}

int Receive(int* tid, char* msg, size_t msglen) { __asm__ volatile("swi #6"); }

int Send(int tid, const char* msg, size_t msglen, char* reply, size_t rplen) {
    __asm__ volatile("swi #5");
}

int Create(int priority, void (*function)()) { __asm__ volatile("swi #4"); }

int MyTid() { __asm__ volatile("swi #3"); }

int MyParentTid() { __asm__ volatile("swi #2"); }

void Exit() { __asm__ volatile("swi #1"); }

void Yield() { __asm__ volatile("swi #0"); }
