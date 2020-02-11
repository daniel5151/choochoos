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

class Kernel final {
    // ------------------ data members -------------------------- //
    std::optional<TaskDescriptor> tasks[MAX_SCHEDULED_TASKS];
    OptArray<Tid, 64> event_queue;
    PriorityQueue<Tid, MAX_SCHEDULED_TASKS> ready_queue;
    Tid current_task;

    // ------------------ private helpers ----------------------- //
    std::optional<Tid> next_tid();
    void reset_task(TaskDescriptor& task);
    void add_to_send_queue(TaskDescriptor& receiver,
                           TaskDescriptor& sender,
                           const char* msg,
                           size_t msglen,
                           char* reply,
                           size_t rplen);
    Tid pop_from_send_queue(TaskDescriptor& receiver,
                            int* sender_tid,
                            char* recv_buf,
                            size_t len);
    int _create_task(int priority,
                     void* function,
                     std::optional<Tid> force_tid);

    // ------------------ syscall handlers ---------------------- //
    int MyTid();
    int MyParentTid();
    int Create(int priority, void* function);
    void Exit();
    void Yield();
    int Send(
        int receiver_tid, const char* msg, int msglen, char* reply, int rplen);
    int Receive(int* tid, char* msg, int msglen);
    int Reply(int tid, const char* reply, int rplen);
    int AwaitEvent(int eventid);

   public:
    Kernel();
    void handle_syscall(uint32_t no, void* user_sp);
    void handle_interrupt();
    std::optional<Tid> schedule();
    void activate(Tid tid);
    void initialize();
    void shutdown();

    size_t num_event_blocked_tasks() const;
};  // class Kernel
}  // namespace kernel
