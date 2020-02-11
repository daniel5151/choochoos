#include <cstring>  // memcpy

#include "kernel/kernel.h"

namespace kernel::handlers {

int Receive(int* tid, char* msg, int msglen) {
    kdebug("Called Receive(tid=%p msg=%p msglen=%d)", (void*)tid, msg, msglen);

    TaskDescriptor& task = tasks[current_task].value();

    switch (task.state.tag) {
        case TaskState::READY: {
            if (!task.send_queue_head.has_value()) {
                task.state = {.tag = TaskState::RECV_WAIT,
                              .recv_wait = {tid, msg, (size_t)msglen}};
                // this will be overwritten when a sender shows up
                return -3;
            }

            return helpers::pop_from_send_queue(task, tid, msg,
                                                (size_t)std::max(msglen, 0));
        }
        default:
            kdebug("Receive() called from task in non-ready state %d",
                   task.state.tag);
            return -1;
    }
}

}  // namespace kernel::handlers

namespace kernel::helpers {

Tid pop_from_send_queue(TaskDescriptor& receiver,
                        int* sender_tid,
                        char* recv_buf,
                        size_t len) {
    kassert(receiver.state.tag == TaskState::READY);
    kassert(receiver.send_queue_head.has_value());

    kassert(tasks[receiver.send_queue_head.value()].has_value());

    TaskDescriptor& sender = tasks[receiver.send_queue_head.value()].value();
    kassert(sender.state.tag == TaskState::SEND_WAIT);

    size_t n = std::min(sender.state.send_wait.msglen, len);
    memcpy(recv_buf, sender.state.send_wait.msg, n);
    *sender_tid = receiver.send_queue_head.value();

    char* reply = sender.state.send_wait.reply;
    size_t rplen = sender.state.send_wait.rplen;
    std::optional<Tid> next = sender.state.send_wait.next;
    sender.state = {.tag = TaskState::REPLY_WAIT, .reply_wait = {reply, rplen}};

    receiver.state = {.tag = TaskState::READY, .ready = {}};

    receiver.send_queue_head = next;
    if (!receiver.send_queue_head.has_value()) {
        receiver.send_queue_tail = std::nullopt;
    } else {
        kassert(receiver.send_queue_tail.has_value());
    }

    return n;
}

}  // namespace kernel::helpers
