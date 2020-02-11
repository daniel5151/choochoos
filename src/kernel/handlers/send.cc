#include <cstring>  // memcpy

#include "kernel/kernel.h"

namespace kernel::handlers {

static void add_to_send_queue(TaskDescriptor& receiver,
                              TaskDescriptor& sender,
                              const char* msg,
                              size_t msglen,
                              char* reply,
                              size_t rplen) {
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
}

int Send(
    int receiver_tid, const char* msg, int msglen, char* reply, int rplen) {
    kdebug("Called Send(tid=%d msg=%p msglen=%d reply=%p rplen=%d)",
           receiver_tid, msg, msglen, reply, rplen);
    if (receiver_tid < 0 || receiver_tid >= MAX_SCHEDULED_TASKS)
        return -1;  // invalid tid
    if (!tasks[receiver_tid].has_value()) return -1;

    Tid sender_tid = current_task;
    TaskDescriptor& sender = tasks[sender_tid].value();
    TaskDescriptor& receiver = tasks[receiver_tid].value();
    switch (receiver.state.tag) {
        case TaskState::SEND_WAIT:
        case TaskState::REPLY_WAIT:
        case TaskState::READY: {
            add_to_send_queue(receiver, sender, msg,
                              (size_t)std::max(msglen, 0), reply,
                              (size_t)std::max(rplen, 0));

            // the sender should never see this - it should be overwritten
            // by Reply()
            return -4;
        }
        case TaskState::RECV_WAIT: {
            size_t n = std::min((size_t)std::max(msglen, 0),
                                receiver.state.recv_wait.len);
            memcpy(receiver.state.recv_wait.recv_buf, msg, n);
            *receiver.state.recv_wait.tid = sender_tid;

            receiver.state = {.tag = TaskState::READY, .ready = {}};
            ready_queue.push(receiver_tid, receiver.priority);

            // set the return value that the receiver gets from Receive() to
            // n.
            TaskDescriptor::write_syscall_return_value(receiver, (int32_t)n);

            sender.state = {.tag = TaskState::REPLY_WAIT,
                            .reply_wait = {reply, (size_t)rplen}};
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
