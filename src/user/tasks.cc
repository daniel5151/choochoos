#include "bwio.h"
#include "syscalls.h"

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
