# New in K2

In K2 we implemented the Send-Receive-Reply mechanism, the Name Server, and the Rock Paper Scissor Server & Client.

## Send-Receive-Reply

SRR is implemented by adding a `state` field to each `TaskDescriptor`, which is the following
tagged union:

```cpp
struct TaskState {
    enum uint8_t { UNUSED, READY, SEND_WAIT, RECV_WAIT, REPLY_WAIT } tag;
    union {
        struct {
        } unused;
        struct {
        } ready;
        struct {
            const char* msg;
            size_t msglen;
            char* reply;
            size_t rplen;
            int next;
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
    };
};
```

So each `TaskDescriptor` can be in any of the states described by `tag`, at which
point the fields in the corresponding struct under the `union` will be populated.
Let's explain each of the states:

- `UNUSED`: the `TaskDescriptor` does not represent a running task.
- `READY`: the task is on the `ready_queue`, waiting to be scheduled.
- `SEND_WAIT`: the task is waiting to `Send()` to another task that hasn't
  called `Receive()` yet.
- `RECV_WAIT`: the task has called `Receive()`, but no task has sent a message to it yet.
- `REPLY_WAIT`: the task called `Send()` and the receiver got the message via `Receive()`, but no other task has called `Reply()` back at the task.


To implement these state transitions, each task has a `send_queue` of tasks that
are waiting to send to it (and thus, must be in the `SEND_WAIT` state). This send
queue is built as an intrusive linked list, where the `next` "pointer" is actually
just another Tid, or `-1` to represent the end of the list. When a task wants to
`Send()` to another task that is not in `RECV_WAIT`, it will be put on the receiver's
send queue (denoted by the fields `send_queue_head` and `send_queue_tail`, also
Tids), and the sending task will be put in `SEND_WAIT`. Note that the `send_wait`
branch of the union contains everything that the sender passed to `Send()`, plus
the `next` pointer, if any other task get added to that same send queue.

If a task is in `SEND_WAIT`, it can only be blocked sending to one receiving task,
so we only need at most one `next` pointer per task. The result is very memory
efficient: by storing one word on each task descriptor, we can support send queues
up to length `MAX_SCHEDULED_TASKS - 1`.

Let's step through the two possibilities for an SRR transaction: sender-first and
receiver-first.

### Sender-first

If a sender arrives first, that means the receiver is not in `RECV_WAIT`, and
therefore does not yet have a `recv_buf` to be filled. If this is the case,
we add the sender to the end of the receiver's send queue, and put the sender in
`SEND_WAIT`.

When the receiver finally calls `Receive()`, it will see that is has a non-empty
send queue. So it will pop the first `TaskDescriptor` off the front of its send
queue, copy the `msg` into `recv_buf`, and transition the sender into `REPLY_WAIT`,
using the same `char* reply` that the sender saved when they went into `SEND_WAIT`.

### Receiver-first

If the receiver arrives first, it checks its send queue. If the send queue is
non-empty, then we follow the same procdure as sender-first, and the new sender
is placed on the send of the receiver's send queue. If the send queue is empty,
the receiver goes into `RECV_WAIT`, holding onto the `recv_buf` to be copied into
once a sender arrives.

When the sender arrives, it notices that the receiver is in `RECV_WAIT`, so it
writes the `msg` directly into `recv_buf`, wakes up the receiver (transitioning
it to `READY` and pushing it onto the `ready_queue`), and goes into `REPLY_WAIT`.

### Reply

`Reply(tid, reply, rplen)` only delivers the reply if the task identified by
`tid` is in `REPLY_WAIT`. The kernel can then immediately copy the `reply` into
the `char* reply` stored in the `reply_wait` branch of the task's `state` union.
This way, reply never blocks - it only returns an error code if the task is not
in `REPLY_WAIT`.

### Error cases

The SRR syscalls return two possible error codes: `-1` and `-2`. `-1` is
returned whenever a `tid` does not represent a task - either it is out of
range, or it points to a TaskDescriptor in the `UNUSED` state. `-2` is returned
in two cases where a SRR transaction cannot be completed:

- If a task tries to `Reply()`to a task that is not in `REPLY_WAIT` - this would
 mean that the task never called `Send()` and thus is not expecting a reply.
- If a task is in `SEND_WAIT` in a receiver's send queue, and the receiver exits.

When a receiver exits, if it has a non-empty send queue, those tasks will never
get a reply because they won't make the transition into `REPLY_WAIT`, and thus
would never wake up. Instead, we iterate through the send queue, waking ever
`SEND_WAIT` task up, and writing the syscall return value of `-2`.

## NameServer

<!-- TODO Prilik -->

## RPS Server & Client

<!-- TODO James -->