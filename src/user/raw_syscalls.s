// Bonus Syscalls

.global __Shutdown
__Shutdown:
    swi #10
    bx lr

.global __Perf
__Perf:
    swi #9
    bx lr

// Core Syscalls

.global __AwaitEvent
__AwaitEvent:
    swi #8
    bx lr

.global __Reply
__Reply:
    swi #7
    bx lr

.global __Receive
__Receive:
    swi #6
    bx lr

.global __Send
__Send:
    swi #5
    bx lr

.global __Create
__Create:
    swi #4
    bx lr

.global __MyTid
__MyTid:
    swi #3
    bx lr

.global __MyParentTid
__MyParentTid:
    swi #2
    bx lr

.global __Exit
__Exit:
    swi #1
    bx lr

.global __Yield
__Yield:
    swi #0
    bx lr
