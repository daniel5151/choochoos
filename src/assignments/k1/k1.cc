#include "common/bwio.h"

#include "user/syscalls.h"

// Note to the TAs:
// We set up the stack such that the initial LR passed to a task points to the
// "Exit" syscall method. This enables omitting a trailing Exit() call, as it
// will be implicitly invoked once the task return.

void OtherTask() {
    int me = MyTid();
    int parent = MyParentTid();
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", me, parent);
    Yield();
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", me, parent);
    // Exit();
}

void TrueFirstUserTask() {
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
}

// FirstUserTask has a priority of 0, and our kernel doesn't support negative
// priorities. Thus, we need to have the FirstUserTask spawn a new "true"
// FirstUserTask with a higher priority.
void FirstUserTask() {
    Create(4, TrueFirstUserTask);
    // Exit();
}
