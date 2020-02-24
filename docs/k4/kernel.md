# ChoochoOS! 🚂

A microkernel RTOS for the TS-7200 Single Board Computer, as used in [CS 452 - Real Time Operating Systems](https://www.student.cs.uwaterloo.ca/~cs452/W20/) at the University of Waterloo.

Written by [Daniel Prilik](https://prilik.com) and [James Hageman](https://jameshageman.com).

## Project Structure

### Building `choochoos`

<!-- TODO: flesh this out -->

- actually include build instructions in this PDF
- mention use of c++2a
    - i.e: we like designated initializers and std::optional
    - look at k3.md

### Navigating the Repo

`choochoos` follows a fairly standard C/C++ project structure, whereby all header files are placed under the `include` directory, with corresponding implementation files placed under the `src` directory.

<!-- TODO: flesh this out -->

- `src` and `include` are separate
- further subdivided into 3 different folders: `kernel`, `user`, and `common`
    - `common` has misc. shared code.
        - e.g: data structures, VT100 helpers, bwio and printf libraries, etc...
    - `user` has userland specific libraries
        - e.g: syscall wrappers, clockserver / uartserver tasks
    - `kernel` has all the juicy kernel bits
        - context switch assembly
        - swi handler, irq handler
        - syscall implementations
        - init / shutdown routines
        - nameserver implementation
- `assignments` is special, with special makefile rules
    - contains the various "userland" implementations

### Kernel Namespaces

<!-- TODO: james -->
<!-- see k4 docs -->

## Initialization

Our entry point is the `_start` routine defined in `src/kernel/boilerplate/crt1.c`. `_start` does couple key things for the initialization of the kernel:
- The link register is stored in the `redboot_return_addr` variable, so we can exit from any point in the kernel's execution by jumping directly to `redboot_return_addr`
- The BSS is zeroed
- All C++ global constructors are run

`_start` proceeds to call `main`, which then immediately calls the `kernel::run()` method.

<!-- TODO: discuss the driver:initialize() method -->
<!-- discuss FirstUserTask (and nameserver) -->

## Main Scheduling Loop

After initialization, the kernel enters it's main scheduling loop. This loop looks roughly as follows:

```cpp
while (true) {
    std::optional<kernel::Tid> next_task = driver::schedule();
    if (next_task.has_value()) {
        driver::activate(next_task.value());
    } else {
        if (driver::num_event_blocked_tasks() == 0) break;
        // start idle measurement
        // ... idle until IRQ ...
        // end idle measurement
        driver::handle_interrupt();
    }
}
```

This loop roughly mirrors the one described in lecture, aside from the fact that `Kernel::activate` does not return a "syscall request" to handle. Instead, the `activate` method implicitly includes syscall handling (see the SWI handler documentation for more details).

## Scheduling

<!-- TODO: james, pls update / double check if this stuff still applies  -->

`schedule()` has a remarkably simple implementation:

```cpp
int schedule() {
    int tid;
    if (ready_queue.pop(tid) == PriorityQueueErr::EMPTY) return -1;
    return tid;
}
```

We have a `ready_queue`, which is a FIFO priority queue of Tids. Tasks that are ready to be executed are on the queue, and `schedule()` simply grabs the next one.

Our priority queue is implemented as a template class in [include/priority_queue.h](../../include/priority_queue.h). A `PriorityQueue<T, N>` is a fixed size object containing up to `N` elements of type `T`, implemented as a modified binary max-heap. Usually, when implementing a priority queue as a binary heap, the elements within the heap are compared only by their priority, to ensure that elements of higher priority are popped first. However, only using an element's priority does not guarantee FIFO ordering _within_ a priority.

Instead, we extend each element in the priority queue to have a `ticket` counter. As elements are pushed onto the queue, they are assigned a monotonically increasing `ticket`. When elements with the same priority are compared, the element with the lower `ticket` is given priority over the element with the higher `ticket`. This has the effect of always prioritizing elements that were added to the queue first, and gives us the desired per-priority FIFO.

Using this ticketed binary heap has both drawbacks and benefits over a fixed-priority implementation (be that a vector of queues, or an intrusive linked list per priority). The benefit is that we can permit `p` priorities in `O(1)` space instead of `O(p)` space. This allows us to define priorities as the entire range of `int`, and will allow us to potentially reliable arithmetic on priorities in the future (however, whether or not we'll need such a thing is yet to be discovered).

The drawback is that we lose FIFO if the ticket counter overflows. Right now, the ticket counter is a `size_t` (a 32-bit unsigned integer), so we would have to push `2^32` `Tid`s onto `ready_queue` in order to see an overflow. This definitely won't happen in our `k1` demo, but it doesn't seem impossible in a longer running program that is rapidly switching tasks. We're experimenting with using the non-native `uint64_t`, and we will profile such a change as we wrap up `k2`.

## Idle Task

<!-- todo -->

<!-- see k3 and k4 (mostly k4) -->

## Context Switching

### Preface: Task State

In our kernel, we've opted to store task state directly on the task's stack. This greatly simplifies task preemption and activation, as user state can be saved/stored using ARM's standard `stmfd` and `ldmfd` methods.

The following diagram describes the layout of a user task's stack after it has been preempted / right before it is activated:

```
+----------- hi mem -----------+
| ... rest of user's stack ... |
|   [ lr                   ]   | /
|   [ r0                   ]   | | Saved General Purpose
|   ...                        | | User registers
|   [ r12                  ]   | \
|   [ ret addr             ]   | - PC to return execution to
|   [ spsr                 ]   | - saved spsr
|         <--- sp --->         |
| ....... unused stack ....... |
+----------- lo mem -----------+
```

Alternatively, it may be useful to think of a user's saved stack pointer as a pointer to the following C-structure:

```c
struct UserStack {
    uint32_t spsr;
    void* ret_addr;
    uint32_t regs[13];
    void* lr;
    uint32_t additional_params[];
};
```

Please keep this structure in mind when referencing the following sections on task activation and preemption.

### Task Activation

Once the scheduler has returned a Tid, the `driver::activate` method is called with the Tid. This method updates the kernel's `current_task` with the provided Tid, fetch the task's saved stack pointer from its Task Descriptor, and hands the stack pointer off to the `_activate_task` assembly routine.

This routine is quite simple, and reproduced here in it's entirety:

```asm
// src/kernel/asm.s

// void* _activate_task(void* next_sp)
_activate_task:
  stmfd   sp!,{r4-r12,lr} // save the kernel's context
  ldmfd   r0!,{r1,r2}     // pop ret addr, spsr from user stack
  msr     spsr,r1         // set spsr to the saved spsr
  stmfd   sp!,{r2}        // push ret val onto kernel stack
  msr     cpsr_c, #0xdf   // switch to System mode
  mov     sp,r0           // install user sp
  ldmfd   sp!,{r0-r12,lr} // restore user registers from sp
  msr     cpsr_c, #0xd3   // switch to Supervisor mode
  ldmfd   sp!,{pc}^       // pop ret addr into pc, `^` updates cpsr
```

The task will then continue its execution until it executes a syscall, or is preempted by a interrupt.

### Context Switching - SWI Handling

Invoking a syscall switches the CPU to Supervisor mode, and jumps execution to the SWI handler. Our SWI handler is written entirely in Assembly, and was given the extremely original name `_swi_handler`.

This routine is a bit more involved than the `_activate_task` routine, mainly due to some tricky register / system-mode juggling. Nonetheless, it is quite short (and heavily commented). It is reproduced here in it's entirety:

```asm
_swi_handler:
  msr     cpsr_c, #0xdf     // switch to System mode
  stmfd   sp!,{r0-r12,lr}   // save user context to user stack
  mov     r4,sp             // hold on to user sp...
  msr     cpsr_c, #0xd3     // switch to Supervisor mode
  mrs     r0,spsr           // get user mode spsr
  stmfd   r4!,{r0, lr}      // ...and stash saved spsr + user return addr
  ldr     r0,[lr, #-4]      // load the last-executed SWI instr into r0...
  bic     r0,r0,#0xff000000 // ...and mask off the top 8 bits to get SWI no
  mov     r1,r4             // copy user sp into r1
  bl      handle_syscall    // call handler method (implemented in C)
  // void handle_syscall(int syscall_no, void* user_sp);
  mov     r0, r4            // return final user SP via r0
  ldmfd   sp!,{r4-r12,pc}   // restore the kernel's context
  // execution returns to caller of _activate_task
```

Execution is returned back to the `driver::activate` method, which proceeds to check to the state of last-executed task. If it's READY, it is queued up to be re-scheduled READY. This completes a single context switch, and execution flow is returned back to the scheduler.

The `swi_handler` routine calls the `handle_syscall` with a task's stack pointer and syscall number. This method is implemented as a small C-stub, which immediately calls the true handle_syscall method (implemented in C++): `driver::handle_syscall()`. At a high level, this method looks roughly as follows:

```cpp
void handle_syscall(uint32_t no, UserStack* user_sp) {
    std::optional<int> ret = std::nullopt;
    using namespace handlers;
    switch (no) {
        case 0: ret = SpecificSyscallHandler(user_sp->regs[0], ...); break;
        case 1: // ...
        // ...
        default: kpanic("invalid syscall %lu", no);
    }

    if (ret.has_value()) {
        user_sp->regs[0] = ret.value();
    }
}
```

Notice how useful the `UserStack*` view is in this case:
- Marshalling arguments to individual syscalls is as simple as accessing struct fields
- Writing a return value to the user task is a simple write to the `regs[0]` field

Please refer to the "Syscall Implementations" section of this documentation for details on the concrete implementation of the various syscall handlers.

### Context Switching - IRQ Handling

<!-- todo: prilik -->

<!-- see k3 docs -->

#### Clock IRQs

<!-- see k3 docs -->

#### UART IRQs

<!-- see k4 docs -->

## Syscall Implementations

<!-- update this -->

- `MyTid()` returns the kernel's `current_tid`, which is updated to the most recent Tid whenever as task is activated.
- `Create(priority, function)` determines the lowest free Tid, and constructs a task descriptor in `tasks[tid]`. The task descriptor is assigned a stack pointer, and the user stack is initialized by casting the stack pointer to a `FreshUserStack*`, and writing to that struct's fields. Notably, we write `stack->lr = (void*)User::Exit`, which sets the return address of `function` to be the `Exit()` syscall, allowing the user to omit the final `Exit()`. The task's `parent_tid` is the current value of `MyTid()`.
- `MyParentTid()` looks up the `TaskDescriptor` by `current_tid`, which holds the parent Tid. (The parent Tid is set to the value of `current_tid` when
- `Yield()` does nothing. Since it leaves the calling task in the `READY` state, the task will be re-added to the `ready_queue` in `activate()`.
- `Exit()` clears the task descriptor at `tasks[MyTid()]`. This prevents the task from being rescheduled in `activate()`, and allows the Tid to be recycled in future calls to `Create()`.

<!-- copy paste docs/k2/kernel2.md - Send-Recieve-Reply -->

<!-- copy paste docs/k3/k3.md - AwaitEvent -->
<!-- maybe flesh it out a bit as well, discuss different interrupt types -->

<!-- copy paste docs/k4/k4.md - New Syscalls -->

## Shutdown

<!-- driver::shutdown routine (for re-executable kernel) -->
<!-- kexit implementation -->

## System Limitations

Our linker script, [ts7200_redboot.ld](ts7200_redboot.ld), defines our allocation of memory. Most notably, we define the range of memory space allocated to user stacks from `__USER_STACKS_START__` to `__USER_STACKS_END__`. Each user task is given 256KiB of stack space, so the maximum number of concurrent tasks in the system is `(__USER_STACKS_END__ - __USER_STACKS_START__) / (256 * 1024)`. However, given the variable size of our BSS and data sections (as we change code), we can't compute the optimal number of concurrent tasks until link time. So instead, we hardcode a `MAX_SCHEDULED_TASKS`, and assert at runtime that no task could possibly have a stack outside of our memory space. Currently, this value is set to 48 tasks.  The kernel is given 512KiB of stack space.

## Common User Tasks

### Name Server

<!-- see k2 docs -->

### Clock Server

<!-- see k3 docs -->

### UART Server

<!-- see k4 docs -->