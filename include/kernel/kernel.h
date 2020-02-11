#pragma once

#include "common/opt_array.h"
#include "common/priority_queue.h"

#include "kernel/helpers.h"
#include "kernel/task_descriptor.h"

namespace kernel {

// defined in the linker script
extern "C" {
// Designate region of memory to use for user stacks
extern char __USER_STACKS_START__, __USER_STACKS_END__;
}

#define USER_STACK_SIZE 0x40000

#define MAX_SCHEDULED_TASKS 48

#define INVALID_PRIORITY -1
#define OUT_OF_TASK_DESCRIPTORS -2

extern std::optional<TaskDescriptor> tasks[MAX_SCHEDULED_TASKS];
extern OptArray<Tid, 64> event_queue;
extern PriorityQueue<Tid, MAX_SCHEDULED_TASKS> ready_queue;
extern Tid current_task;

namespace helpers {

int create_task(int priority, void* function, std::optional<Tid> force_tid);

}  // namespace helpers

namespace handlers {

int MyTid();
int MyParentTid();
int Create(int priority, void* function);
void Exit();
void Yield();
int Send(int receiver_tid, const char* msg, int msglen, char* reply, int rplen);
int Receive(int* tid, char* msg, int msglen);
int Reply(int tid, const char* reply, int rplen);
int AwaitEvent(int eventid);

}  // namespace handlers

namespace driver {

void handle_syscall(uint32_t no, void* user_sp);
void handle_interrupt();
std::optional<Tid> schedule();
void activate(Tid tid);
void initialize();
void shutdown();
size_t num_event_blocked_tasks();

}  // namespace driver

int run();

}  // namespace kernel
