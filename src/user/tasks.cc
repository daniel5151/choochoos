#include "bwio.h"
#include "user/syscalls.h"

void OtherTask() {
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", MyTid(), MyParentTid());
    Yield();
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", MyTid(), MyParentTid());
    Exit();
}

void FirstUserTask() {
    Create(3, OtherTask);
    Create(3, OtherTask);
    Create(5, OtherTask);
    Create(5, OtherTask);
    bwputstr(COM2, "FirstUserTask: exiting\r\n");
    Exit();
}

void DummyTask() {
    bwprintf(COM2, "hello from the dummy task!\r\n");
    Yield();
}
