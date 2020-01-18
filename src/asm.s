.global _activate_task
.global _swi_handler

_swi_handler:
    // Switch to system mode (IRQs disabled)
    // This banks in the user's LR and SP
    msr     cpsr_c, #0xdf

    // Stack user registers on user stack
    stmfd   sp!,{r0-r12,lr}
    mov     r4,sp // hold on to user sp

    // Switch to supervisor mode (IRQs disabled)
    // This banks in the kernel's SP and LR.
    msr     cpsr_c, #0xd3

    // get user mode lr (which is in lr_svc)
    mov     r0,lr
    // get user mode spsr
    mrs     r1,spsr
    // and stack both of them on the user stack
    stmfd   r4!,{r0}
    stmfd   r4!,{r1}

    // r0 = khandlesyscall 1st param = syscall number
    ldr     r0,[r0, #-4]      // Load the last-executed SWI instr into r0...
    bic     r0,r0,#0xff000000 // ...and mask off the top 8 bits to get SWI no
    // r1 = khandlesyscall 2nd param = pointer to user's registers + CPSR
    mov     r1,r4
    bl      handle_syscall
    // r0 now contains the syscall return value
    // we'll store it alongside the rest of the user's state
    stmfd   r4!, {r0}

    // -------- HIGH mem
    // ... rest of user's stack
    // [ r0   ]
    // [ r... ]
    // [ r12  ]
    // [ lr   ]
    // [ spsr ]
    // [ r0   ] (syscall retval)
    // -------- LOW mem (sp)

    // Return the final user SP via r0
    mov     r0, r4

    // Restore the kernel's context, and return to the caller of _activate_task
    ldmfd   sp!,{r4-r12,pc}

// void* _activate_task(void* next_sp)
// returns final SP after _swi_handler is finished
_activate_task:
    // save the kernel's context
    stmfd   sp!,{r4-r12,lr}

    // Switch to system mode (IRQs disabled)
    msr     cpsr_c, #0xdf
    // move provided SP into sp
    mov     sp,r0

    ldmfd sp!,{r0} // syscall return value
    ldmfd sp!,{r1} // spsr
    ldmfd sp!,{r2} // swi return address

    // restore user registers from stack
    add   sp,sp,#16 // skip popping r0 through r3
    ldmfd sp!,{r4-r12,lr}

    // set the spsr to the user's saved spsr
    msr   spsr,r1
    movs  pc,r2
