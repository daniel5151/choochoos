#pragma once

#include <variant>

#include "common/opt_array.h"
#include "common/priority_queue.h"

#include "kernel/helpers.h"
#include "kernel/task_descriptor.h"
#include "kernel/volatile_data.h"

namespace user {
#include "user/syscalls.h"
}

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

using TidOrVolatileData = std::variant<Tid, VolatileData>;

// kernel state
extern std::optional<TaskDescriptor> tasks[MAX_SCHEDULED_TASKS];
extern OptArray<TidOrVolatileData, 64> event_queue;
extern PriorityQueue<Tid, MAX_SCHEDULED_TASKS> ready_queue;
extern Tid current_task;

namespace perf {
extern uint32_t last_perf_call_time;
namespace idle_time {
extern uint32_t pct;
extern uint32_t counter;
}  // namespace idle_time
}  // namespace perf

namespace helpers {

int create_task(int priority, void* function, std::optional<Tid> force_tid);

}  // namespace helpers

namespace handlers {

void Shutdown();
void Perf(user::perf_t* perf);

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
