# New in K2

In K2 we implemented the Send-Receive-Reply mechanism, the Name Server, and the Rock Paper Scissor Server & Client.

## Send-Receive-Reply

SRR is implemented by adding a `TaskState state;` field to each `TaskDescriptor`. `TaskState` is a tagged union with the following structure:

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
point the fields in the corresponding struct under the `union` will be populated:

- `UNUSED`: the `TaskDescriptor` does not represent a running task.
- `READY`: the task is on the `ready_queue`, waiting to be scheduled.
- `SEND_WAIT`: the task is waiting to `Send()` to another task that hasn't
  called `Receive()` yet.
- `RECV_WAIT`: the task has called `Receive()`, but no task has sent a message to it yet.
- `REPLY_WAIT`: the task called `Send()` and the receiver got the message via `Receive()`, but no other task has called `Reply()` back at the task.

To implement these state transitions, each task has a `send_queue` of tasks that
are waiting to send to it (and must therefore be in the `SEND_WAIT` state). This send
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
non-empty, then we follow the same procedure as sender-first, and the new sender
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

## Name Server

The Name Server task provides tasks with a simple to use mechanism to discover one another's Tids.

### Public Interface

The name server's public interface can be found under `include/user/tasks/nameserver.h`, a header file which defines the methods and constants other tasks can use to set up and communicate with the nameserver.

We've decided to use an incredibly simple closure mechanism for determining the Tid of the name server, namely, we enforce that the name server is the second user task to be spawned (after the FirstUserTask), resulting in it's Tid always being 1.

To make working with the name server easier, instead of having tasks communicate with it directly via SRR system calls, a pair of utility methods are provided which streamline the registration and query flows:

- `int WhoIs(const char*)`: returns the Tid of a task with a given name, `-1` if the name server task is not initialized, or `-2` if there was no registered task with the given name.
- `int RegisterAs()`: returns `0` on success, `-1` if the name server task is not initialized, or `-2` if there was an error during name registration (e.g: nameserver is at capacity.)
    - _Note:_ as described later, the nameserver currently panics if it runs out of space, so user tasks will never actually get back `-2`.

### Implementation

The name server's implementation can be found under `src/user/tasks/nameserver.cc`.

#### Preface: A Note on Error Handling

At the moment, the name server's implementation will simply panic if various edge-conditions are encountered, namely, when the server runs out of space in any of it's buffers. While it would have been possible to return an error code to the user, we decided not to, as it's unlikely that the user process would be able to gracefully recover from such an error.

Instead, we will be tweaking the size of our name server's buffers over the course of development, striking a balance between memory usage and availability. Alternatively, we may explore implementing userspace heaps at some point, in which case, we may be able to grow the buffer on-demand when these edge conditions are hit.

#### Associating Strings with Tids

The core of any name server is some sort of associative data structure to associate strings to Tids.

While there are many different data structures that fit this requirement, ranging from Tries, Hash Maps, BTree Maps, etc..., we've decided to use a simple, albeit potentially inefficient data structure instead: a plain old fixed-length array of ("String", Tid) pairs. This simple data structure provides O(1) registration, and O(n) lookup, which shouldn't be too bad, as we assume that most user applications won't require too many named tasks (as reflected in the specification's omission of any sort of "de-registration" functionality).

##### Aside: Efficiently Storing Names

Note that we've put the term "String" in quotes. This is because instead of using a fixed-size char buffer for each pair, we instead allocate strings via a separate data structure: the `StringArena`.

`StringArena` is a simple class which contains a fixed-size `char` buffer, and a index to the tail of the buffer. It exposes two methods:

- `size_t StringArena::add(const char* s, const size_t n)`: Copy a string of length `n` into the arena, returning a handle to the string's location within the arena (represented by a `size_t`). This handle can then be passed to the second method on the arena...
- `const char* StringArena::get(const size_t handle)`: Return a pointer to a string associated with the given handle, or `nullptr` if the handle is invalid.

Whenever `add` is called, the string is copied into the arena's fixed-size internal buffer, incrementing the tail-index to point past the end of the string. `get` simply returns a pointer into the char buffer associated with the returned handle (which at the moment, is simply an index into the char array).

The `StringArena` approach allows us to avoid having to put an explicit limit on the size of name strings, as strings of varying lengths can be "packed" together in the single char buffer.

#### Incoming and Outgoing Messages

The `WhoIs` and `RegisterAs` functions abstract over the name server's message interface, which is comprised of two tagged unions: `Request` and `Response`.

```cpp
enum class MessageKind : size_t { WhoIs, RegisterAs, Shutdown };

struct Request {
    MessageKind kind;
    union {
        struct {
            char name[NAMESERVER_MAX_NAME_LEN];
            size_t len;
        } who_is;
        struct {
            char name[NAMESERVER_MAX_NAME_LEN];
            size_t len;
            int tid;
        } register_as;
    };
};

struct Response {
    MessageKind kind;
    union {
        struct {
        } shutdown;
        struct {
            bool success;
            int tid;
        } who_is;
        struct {
            bool success;
        } register_as;
    };
};
```

There are 3 types of request, each with a corresponding response
- `Shutdown` - terminate the name server task
- `WhoIs` - Return the Tid associated with a given name
- `RegisterAs` - Register a Tid with a given name

The latter two messages return a non-empty response, indicating if the operation was successful, and for `WhoIs`, a Tid (if one was found).

The server uses a standard message handling loop, whereby the body of the server task is an infinite loop, which continuously waits for incoming messages, switches on their type, and handles them accordingly.

#### Future Improvements

We would like to split up the Request into two parts: a request "header", followed by an optional request "body." This would lift the artificially imposed `NAMESERVER_MAX_NAME_LEN` limit on names, as the request header could specify the length of the upcoming message, which the name server could then read into a variable-sized stack-allocated array (allocated via `alloca` / a VLA). This comes with a minor security risk, whereby a malicious and/or misbehaving task could specify a extremely large name, and overlow the nameserver's stack, but given the the operating system will only be running code we write ourselves, this shouldn't be too much of an issue.
