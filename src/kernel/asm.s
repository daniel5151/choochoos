.global _swi_handler
_swi_handler:
    // Switch to system mode (IRQs disabled)
    // This banks in the user's LR and SP
    msr     cpsr_c, #0xdf

    // set aside some space to hold the user return address
    sub     sp,sp,#4

    // Stack user registers on user stack
    stmfd   sp!,{r0-r12,lr}
    mov     r4,sp // hold on to user sp

    // Switch to supervisor mode (IRQs disabled)
    // This banks in the kernel's SP and LR.
    msr     cpsr_c, #0xd3

    // store user mode spsr
    mrs     r0,spsr
    stmfd   r4!,{r0}

    // store user mode return address (which is in lr_svc)
    mov     r0,lr
    str     r0,[r4, #60] // 14 * 4 + 4

    // r0 = khandlesyscall 1st param = syscall number
    ldr     r0,[r0, #-4]      // Load the last-executed SWI instr into r0...
    bic     r0,r0,#0xff000000 // ...and mask off the top 8 bits to get SWI no
    // r1 = khandlesyscall 2nd param = pointer to user's registers + CPSR
    mov     r1,r4
    bl      handle_syscall
    // handle_syscall writes the syscall return value directly into the user's
    // stack (i.e: overwriting the value of the saved r0 register)

    // At this point, the user's stack looks like this:
    //
    // +----------- hi mem -----------+
    // | ... rest of user's stack ... |
    // |   [ ret addr             ]   |
    // |   [ lr                   ]   |
    // |   [ r0                   ]   |
    // |   ...                        |
    // |   [ r12                  ]   |
    // |   [ spsr                 ]   |
    // |         <--- sp --->         |
    // | ....... unused stack ....... |
    // +----------- lo mem -----------+

    // Return the final user SP via r0
    mov     r0, r4

    // Restore the kernel's context, and return to the caller of _activate_task
    ldmfd   sp!,{r4-r12,pc}

// void* _activate_task(void* next_sp)
// returns final SP after _swi_handler is finished (via _swi/irq_handler method)
.global _activate_task
_activate_task:
    // save the kernel's context
    stmfd   sp!,{r4-r12,lr}

    // Switch to system mode (IRQs disabled)
    msr     cpsr_c, #0xdf
    // move provided SP into sp
    mov     sp,r0

    // get spsr off stack
    ldmfd   sp!,{r0}

    // set the spsr to the user's saved spsr
    msr     spsr,r0

    // restore user registers from stack
    ldmfd   sp!,{r0-r12,lr,pc}^

.global _enable_caches
_enable_caches:
    mrc     p15, 0, r1, c1, c0, 0  // read config register
    orr     r1, r1, #0x1 << 12     // enable instruction cache
    orr     r1, r1, #0x1 << 2      // enable data cache

    mcr     p15, 0, r0, c7, c7, 0  // Invalidate entire instruction + data caches
    mcr     p15, 0, r1, c1, c0, 0  // enable caches

    bx      lr

.global _disable_caches
_disable_caches:
    mrc     p15, 0, r1, c1, c0, 0 // Read config register
    bic     r1, r1, #0x1 << 12    // instruction cache disable
    bic     r1, r1, #0x1 << 2     // data cache disable

    mcr     p15, 0, r1, c1, c0, 0  // disable caches

    bx      lr
