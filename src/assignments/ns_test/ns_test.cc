#include "user/debug.h"
#include "user/syscalls.h"

void T1() { RegisterAs("Task1"); }
void T2() { RegisterAs("TASK_2"); }
void T3() { RegisterAs("task 3!!!"); }

void FirstUserTask() {
    int t1 = Create(1, T1);

    assert(WhoIs("Task1") == t1);
    assert(WhoIs("TASK_2") == -2);
    assert(WhoIs("task 3!!!") == -2);

    int t2 = Create(1, T2);
    int t3 = Create(1, T3);

    assert(WhoIs("Task1") == t1);
    assert(WhoIs("TASK_2") == t2);
    assert(WhoIs("task 3!!!") == t3);
    assert(WhoIs("???") == -2);

    bwprintf(COM2, "ok" ENDL);
}
