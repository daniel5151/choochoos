#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

int Reply(int tid, const char* reply, int rplen) { __asm__ volatile("swi #7"); }

int Receive(int* tid, char* msg, int msglen) { __asm__ volatile("swi #6"); }

int Send(int tid, const char* msg, int msglen, char* reply, int rplen) {
    __asm__ volatile("swi #5");
}

int Create(int priority, void (*function)()) { __asm__ volatile("swi #4"); }

int MyTid() { __asm__ volatile("swi #3"); }

int MyParentTid() { __asm__ volatile("swi #2"); }

void Exit() { __asm__ volatile("swi #1"); }

void Yield() { __asm__ volatile("swi #0"); }
