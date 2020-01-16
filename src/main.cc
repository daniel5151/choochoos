#include <cstddef>

#include "kernel.h"
#include "priority_queue.h"
#include "ts7200.h"

#define STACK_SPACE_PER_TASK 0x10000
// TODO optimize this, taking into account our available RAM and the amount of
// stack we need for the kernel.
#define MAX_SCHEDULED_TASKS 16

#define MAX_PRIORITY 8  // 0 <= priority < 8
#define MAX_TASKS_PER_PRIORITY 8
#define INVALID_PRIORITY -1
#define OUT_OF_TASK_DESCRIPTORS -2

struct TaskDef {
    bool active;
    void (*resume_ptr)();
    int parent_tid;
};

TaskDef tasks[MAX_SCHEDULED_TASKS] = {{0}};
PriorityQueue<int /* task descriptor */, MAX_PRIORITY, MAX_TASKS_PER_PRIORITY>
    ready_queue;

int current_task = -1;

int MyTid() { return current_task; }
int MyParentTid() { return tasks[MyTid()].parent_tid; }

int next_tid() {
    for (int tid = 0; tid < MAX_SCHEDULED_TASKS; tid++) {
        if (!tasks[tid].active) return tid;
    }
    return -1;
}

int Create(int priority, void (*function)()) {
    if (priority < 0 || priority >= MAX_PRIORITY) return INVALID_PRIORITY;
    int tid = next_tid();
    if (tid < 0) return OUT_OF_TASK_DESCRIPTORS;

    if (ready_queue.push(tid, priority) == PriorityQueueErr::FULL) {
        kpanic("out of space in ready queue (tid=%d", tid);
    }

    bwprintf(COM2, "Created: %d\r\n", tid);

    tasks[tid] = (TaskDef){
        .active = true, .resume_ptr = function, .parent_tid = MyTid()};

    return tid;
}

void Exit() {
    int tid = MyTid();
    tasks[tid] = (TaskDef){.active = false, .resume_ptr = 0, .parent_tid = -1};
}

void Yield() {
    // TODO context switch back to the kernel
}

void OtherTask() {
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", MyTid(), MyParentTid());
    Yield();
    bwprintf(COM2, "MyTid=%d MyParentTid=%d\r\n", MyTid(), MyParentTid());
}

void FirstUserTask() {
    Create(3, OtherTask);
    Create(3, OtherTask);
    Create(5, OtherTask);
    Create(5, OtherTask);
    bwputstr(COM2, "FirstUserTask: exiting\r\n");
    Exit();
}

void initialize() {
    int tid = Create(4, FirstUserTask);
    if (tid < 0) kpanic("could not create tasks (error code %d)", tid);
}

int schedule() {
    int tid;
    if (ready_queue.pop(tid) == PriorityQueueErr::EMPTY) return -1;
    return tid;
}

void activate(int tid) {
    current_task = tid;
    tasks[tid].resume_ptr();
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    bwsetfifo(COM2, false);
    bwprintf(COM2, "Hello from the kernel!\r\n");

    initialize();

    while (true) {
        int curtask = schedule();
        if (curtask < 0) break;
        activate(curtask);
    }

    return 0;
}
