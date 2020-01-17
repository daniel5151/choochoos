.global _activate_task
.global _swi_handler

_swi_handler:
    // Stack user registers on user stack
    stmfd   sp!,{r0-r11}^ // < doesn't seem to work
    mov     r0,lr // user LR is actually placed into svc_lr...
    stmfd   sp!,{r0}^

    // Stack the SPSR register (with the user's CPSR)
    mrs     r4,spsr
    stmfd   sp!,{r4}^

    // Switch to system mode (IRQs disabled)
    // This banks in the user's LR and SP
    msr     cpsr_c, #0xdf

    // r0 = khandlesyscall 1st param = syscall number
    ldr     r0,[r0, #-4]      // Load the last-executed SWI instr into r0...
    bic     r0,r0,#0xff000000 // ...and mask off the top 8 bits to get SWI no
    // r1 = khandlesyscall 2nd param = pointer to user's registers + CPSR
    mov     r1,sp

    // At this point, the entire user state is saved on the user's stack,
    // with the user's stack pointer transiently stored in r1

    // Call the syscall handler with the syscall number and user sp as arguments
    bl      handle_syscall
    // Stack the return value of the system call, as there's no guarantee that
    // the task will be immediately rescheduled
    stmfd   sp!, {r0}

    // -------- HIGH mem
    // ... rest of user's stack
    // [ r0   ]
    // [ r... ]
    // [ r11  ]
    // [ lr   ]
    // [ spsr ]
    // [ r0   ] (syscall retval)
    // -------- LOW mem (sp)

    // Store the final user SP in r0
    mov     r0, sp
    // Switch to supervisor mode (IRQs disabled)
    // This banks in the kernel's SP and LR.
    msr     cpsr_c, #0xd3
    // Restore the kernel's context, and return to the caller of _activate_task
    ldmfd sp!,{r4-r11,pc}

// void* _activate_task(void* next_sp)
// returns final SP after _swi_handler is finished
_activate_task:
    // save the kernel's context
    stmfd   sp!,{r4-r11,lr}

    // Switch to system mode (IRQs disabled)
    msr     cpsr_c, #0xdf
    // move provided SP into sp
    mov     sp,r0

    // store syscall return value in r0, and the user spsr in r1
    ldmfd sp!,{r0}
    ldmfd sp!,{r1}

    // restore user registers from stack
    ldmfd sp!,{lr}
    sub   sp,sp,#16 // skip popping r0 through r3
    ldmfd sp!,{r4-r11}

    // set the spsr to the user's saved spsr
    msr   spsr,r1
    movs  pc,lr
