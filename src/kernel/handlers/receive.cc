#include <cstring>  // memcpy

#include "kernel/kernel.h"

namespace kernel::handlers {

int Receive(int* sender_tid, char* msg, int msglen) {
    kdebug("Called Receive(tid=%p msg=%p msglen=%d)", (void*)sender_tid, msg,
           msglen);

    TaskDescriptor& receiver = tasks[current_task].value();

    switch (receiver.state.tag) {
        case TaskState::READY: {
            if (!receiver.send_queue_head.has_value()) {
                receiver.state = {
                    .tag = TaskState::RECV_WAIT,
                    .recv_wait = {sender_tid, msg, (size_t)msglen}};
                // this will be overwritten when a sender shows up
                return -3;
            }

            size_t len = (size_t)std::max(msglen, 0);

            kassert(receiver.state.tag == TaskState::READY);
            kassert(receiver.send_queue_head.has_value());

            kassert(tasks[receiver.send_queue_head.value()].has_value());

            TaskDescriptor& sender =
                tasks[receiver.send_queue_head.value()].value();
            kassert(sender.state.tag == TaskState::SEND_WAIT);

            size_t n = std::min(sender.state.send_wait.msglen, len);
            if (msg != nullptr && sender.state.send_wait.msg != nullptr) {
                memcpy(msg, sender.state.send_wait.msg, n);
            }
            if (sender_tid != nullptr) {
                *sender_tid = receiver.send_queue_head.value();
            }

            char* reply = sender.state.send_wait.reply;
            size_t rplen = sender.state.send_wait.rplen;
            std::optional<Tid> next = sender.state.send_wait.next;
            sender.state = {.tag = TaskState::REPLY_WAIT,
                            .reply_wait = {reply, rplen}};

            receiver.state = {.tag = TaskState::READY, .ready = {}};

            receiver.send_queue_head = next;
            if (!receiver.send_queue_head.has_value()) {
                receiver.send_queue_tail = std::nullopt;
            } else {
                kassert(receiver.send_queue_tail.has_value());
            }

            return n;
        }
        default:
            kdebug("Receive() called from task in non-ready state %d",
                   receiver.state.tag);
            return -1;
    }
}

}  // namespace kernel::handlers
