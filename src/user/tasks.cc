#include "bwio.h"
#include "user/syscalls.h"

void OtherTask() {
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", MyTid(), MyParentTid());
    Yield();
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", MyTid(), MyParentTid());
    // Exit();
}

void FirstUserTask() {
    int tid;
    tid = Create(3, OtherTask);
    bwprintf(COM2, "Created: %d\r\n", tid);

    tid = Create(3, OtherTask);
    bwprintf(COM2, "Created: %d\r\n", tid);

    tid = Create(5, OtherTask);
    bwprintf(COM2, "Created: %d\r\n", tid);

    tid = Create(5, OtherTask);
    bwprintf(COM2, "Created: %d\r\n", tid);

    bwputstr(COM2, "FirstUserTask: exiting\r\n");

    // Exit();
}
