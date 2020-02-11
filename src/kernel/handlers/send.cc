#include <cstring>  // memcpy

#include "kernel/kernel.h"

namespace kernel::handlers {

int Send(int receiver_tid, const char* msg, int mlen, char* reply, int rlen) {
    kdebug("Called Send(tid=%d msg=%p msglen=%d reply=%p rplen=%d)",
           receiver_tid, msg, msglen, reply, rplen);
    if (receiver_tid < 0 || receiver_tid >= MAX_SCHEDULED_TASKS)
        return -1;  // invalid tid
    if (!tasks[receiver_tid].has_value()) return -1;

    size_t msglen = (size_t)std::max(mlen, 0);
    size_t rplen = (size_t)std::max(rlen, 0);

    Tid sender_tid = current_task;
    TaskDescriptor& sender = tasks[sender_tid].value();
    TaskDescriptor& receiver = tasks[receiver_tid].value();

    switch (receiver.state.tag) {
        case TaskState::SEND_WAIT:
        case TaskState::REPLY_WAIT:
        case TaskState::READY: {
            kassert(receiver.state.tag != TaskState::RECV_WAIT);
            kassert(sender.state.tag == TaskState::READY);

            sender.state = {.tag = TaskState::SEND_WAIT,
                            .send_wait = {.msg = msg,
                                          .msglen = msglen,
                                          .reply = reply,
                                          .rplen = rplen,
                                          .next = std::nullopt}};
            if (!receiver.send_queue_head.has_value()) {
                kassert(!receiver.send_queue_tail.has_value());

                receiver.send_queue_head = sender.tid;
                receiver.send_queue_tail = sender.tid;
            } else {
                kassert(receiver.send_queue_head.has_value());
                kassert(receiver.send_queue_tail.has_value());

                kassert(tasks[receiver.send_queue_tail.value()].has_value());

                TaskDescriptor& old_tail =
                    tasks[receiver.send_queue_tail.value()].value();
                kassert(old_tail.state.tag == TaskState::SEND_WAIT);
                kassert(!old_tail.state.send_wait.next.has_value());
                old_tail.state.send_wait.next = sender.tid;
                receiver.send_queue_tail = sender.tid;
            }

            // the sender should never see this - it should be overwritten
            // by Reply()
            return -4;
        }
        case TaskState::RECV_WAIT: {
            size_t n = std::min(msglen, receiver.state.recv_wait.len);
            memcpy(receiver.state.recv_wait.recv_buf, msg, n);
            *receiver.state.recv_wait.tid = sender_tid;

            receiver.state = {.tag = TaskState::READY, .ready = {}};
            ready_queue.push(receiver_tid, receiver.priority);

            // set the return value that the receiver gets from Receive() to
            // n.
            TaskDescriptor::write_syscall_return_value(receiver, (int32_t)n);

            sender.state = {.tag = TaskState::REPLY_WAIT,
                            .reply_wait = {reply, rplen}};
            // the sender should never see this - it should be overwritten
            // by Reply()
            return -3;
        }

        default:
            kpanic("invalid state %d for task %d", (int)receiver.state.tag,
                   receiver_tid);
    }
}

}  // namespace kernel::handlers
