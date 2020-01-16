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

    // maybe we need these? idk
    size_t stack_start;
    size_t stack_end;
};

TaskDef tasks[MAX_SCHEDULED_TASKS] = {{.active = false, 0}};
PriorityQueue<int /* task descriptor */, MAX_PRIORITY, MAX_TASKS_PER_PRIORITY>
    ready_queue;

int parent_task = -1;
int current_task = -1;

int MyTid() { return current_task; }
int MyParentTid() { return parent_task; }

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

    tasks[tid].active = true;
    tasks[tid].resume_ptr = function;

    return tid;
}

void main_task() {
    bwprintf(COM2, "Hello from the main task! (MyTid=%d, MyParentTid=%d)\r\n",
             MyTid(), MyParentTid());
}

void initialize() {
    int tid = Create(1, main_task);
    if (tid < 0) kpanic("could not create tasks (error code %d)", tid);
}

int schedule() {
    int tid;
    if (ready_queue.pop(tid) == PriorityQueueErr::EMPTY) return -1;
    return tid;
}

void resume(int tid) {
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
        int tid = schedule();
        if (tid < 0) kpanic("Out of tasks to run, shutting down");
        resume(tid);
    }

    return 0;
}
