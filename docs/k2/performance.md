# Performance Measurement

## Implementation

The implementation for this test suite can be found under the `src/assignments/k2_profile/` folder.

While it would have been possible to construct the test executable such that it would run tests with/without caches in a single binary, we opted to enable caches by default, a disable them with a compile-time flag. Enabling the caches from user-mode would have been quite the hassle, as caches can only be enabled while in a privileged execution mode. Instead, we opted to use makefile variables and pre-processor directives to enable/disable caches and/or optimizations at compile time.

The final results were collected via 4 different executables:
- opt nocache - `make TARGET=k2_profile NENABLE_CACHES=1`
- noopt nocache - `make TARGET=k2_profile NO_OPTIMIZATION=1 NENABLE_CACHES=1`
- opt cache - `make TARGET=k2_profile`
- noopt nocache - `make TARGET=k2_profile NO_OPTIMIZATION=1`

The actual structure of the performance measurement tasks is fairly straightforward, whereby the first user task loops through the various parameter combinations, spawns the sender / receiver tasks, provides them with parameters, and waits for them to finish a round of message-passing. The FirstUserTask sets up the 508Khz Timer 3 to measure the time between passing the spawned tasks's their parameters, to the time when both tasks complete their benchmarking iterations.

We use an arbitrary value for the number of message passing iterations to perform, currently set to `4096 * 4`. We found that this number of iterations provided a long-enough execution time such that any measurement inaccuracies (such as not having a sub-microsecond precision clock) are drowned out.

## Results

A copy of `performance.txt`'s contents is included here for convenience and easy reference.

```
noopt nocache R 4 10500
noopt nocache R 64 10832
noopt nocache R 256 11960
noopt nocache S 4 11208
noopt nocache S 64 11544
noopt nocache S 256 12668
opt nocache R 4 3316
opt nocache R 64 3644
opt nocache R 256 4772
opt nocache S 4 3360
opt nocache S 64 3692
opt nocache S 256 4820
noopt cache R 4 716
noopt cache R 64 736
noopt cache R 256 800
noopt cache S 4 760
noopt cache S 64 780
noopt cache S 256 844
opt cache R 4 228
opt cache R 64 252
opt cache R 256 320
opt cache S 4 228
opt cache S 64 256
opt cache S 256 320
```

## Observations

### Enabling Caches > Enabling Optimizations

We begin with a fairly obvious observation: Enabling optimizations improves execution speed, and enabling caches improve execution speed. This should be fairly obvious, given that's what the flags / hardware is there for. What's more interesting is _just how much_ each contributes to the overall performance of the system.

Enabling optimizations resulted in a roughly 3x improvement in execution time with caches disabled, and a 4x increase with caches enabled. This is quite the improvement, but it but pales in comparison to the whopping 10x-16x performance increase when caching is enabled! But why?

Optimizations typically reduce the number of operations a CPU has to perform, reduce the number of memory accesses a CPU must perform, and reorganize instructions to make better use of the CPU's instruction pipeline. These are all valuable changes, resulting in notable and noticeable performance improvements to parts of a program that the programmer has not optimized fully. That said, most optimizations only improve the performance of a few localized parts of a user's program, with compiler running multiple optimization passes to optimize as much of a user's program as possible.

Caches, on the other hand, optimize almost _every single part_ of a user's program. Memory accesses are an extremely common operation, and improving their performance translates to a massive improvements execution speed.

This benchmark in particular is all about writing and reading to/from memory, which isn't a particularly complicated series of CPU operations, and can only be optimized so much.

### Cache Performance Hit from 4/64 to 256 Byte Message Sizes

Looking at the results, increasing the message size by a factor of 16x from 4 to 64 resulted in a much lower performance hit than the smaller increase by a factor of 4x from 64 to 256. But Why?

As specified in Chapter 2-1 of the manual, the EP9302 SoC's cache lines are 8-words-- or 64-bytes-- wide. The 4 byte and 64 byte message sizes both fit cleanly into a single cache line, while the 256 byte message spills across multiple cache lines.

### Receiver Blocks First > Sender Blocks First

There is a small performance increase in the cases where the receiver has a higher priority (therefore blocking first). The reason for this can be explained by examining the state-transitions the sender and receiver tasks undergo in each of the two cases:

- Case 1: Receiver Blocks First
    - When the receiver task blocks first, it is put into the RECV_WAIT state, waiting for someone to send it a message.
    - When the sender task calls send, the kernel notices that the receiver task is already waiting for a message, and immediately performs the Send-Receive transaction. The Receiver task is put back into the READY state, while the sender is put into the REPLY_WAIT state.
- Case 2: Receiver Blocks First
    - When the sender task blocks first, it is put into the SEND_WAIT state, as it's receiver isn't currently blocked waiting for a message.
    - When the receiver task calls receive, the kernel notices that there is already a task waiting to send a message, immediately performs the Send-Receive transaction. The Receiver task is put back into the READY state, while the sender is put into the REPLY_WAIT state.

Notice that in the first case, there is one-less state transition than the latter task, whereby the sender doesn't have to be put in the SEND_WAIT state whatsoever. This results in less code being executed to complete a full SRR operation, translating to a bump in performance.
