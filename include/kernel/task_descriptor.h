#pragma once

#include <optional>

#include "kernel/tid.h"

namespace kernel {
struct TaskState {
    enum uint8_t { READY, SEND_WAIT, RECV_WAIT, REPLY_WAIT, EVENT_WAIT } tag;
    union {
        struct {
        } ready;
        struct {
            const char* msg;
            size_t msglen;
            char* reply;
            size_t rplen;
            std::optional<Tid> next;
        } send_wait;
        struct {
            int* tid;
            char* recv_buf;
            size_t len;
        } recv_wait;
        struct {
            char* reply;
            size_t rplen;
        } reply_wait;
        struct {
        } event_wait;
    };
};

struct TaskDescriptor {
    Tid tid;
    std::optional<Tid> send_queue_head;
    std::optional<Tid> send_queue_tail;
    size_t priority;
    TaskState state;
    std::optional<Tid> parent_tid;
    void* sp;
};

TaskDescriptor new_task(Tid tid,
                        size_t priority,
                        std::optional<Tid> parent_tid,
                        void* stack_ptr);

void write_syscall_return_value(TaskDescriptor& task, int32_t value);

}  // namespace kernel
