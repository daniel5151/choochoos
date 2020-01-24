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

    // get user mode return address (which is in lr_svc)
    mov     r0,lr
    // get user mode spsr
    mrs     r1,spsr
    // and stack both of them on the user stack
    stmfd   r4!,{r0,r1}

    // r0 = khandlesyscall 1st param = syscall number
    ldr     r0,[r0, #-4]      // Load the last-executed SWI instr into r0...
    bic     r0,r0,#0xff000000 // ...and mask off the top 8 bits to get SWI no
    // r1 = khandlesyscall 2nd param = pointer to user's registers + CPSR
    mov     r1,r4
    bl      handle_syscall
    // r0 now contains the syscall return value
    // we'll store it alongside the rest of the user's state
    stmfd   r4!, {r0}

    // At this point, the user's stack looks like this:
    //
    // +----------- hi mem -----------+
    // | ... rest of user's stack ... |
    // |   [ lr                   ]   |
    // |   [ r0                   ]   |
    // |   ...                        |
    // |   [ r12                  ]   |
    // |   [ spsr                 ]   |
    // |   [ ret addr             ]   |
    // |   [ syscall response     ]   |
    // |         <--- sp --->         |
    // | ....... unused stack ....... |
    // +----------- lo mem -----------+

    // Return the final user SP via r0
    mov     r0, r4

    // Restore the kernel's context, and return to the caller of _activate_task
    ldmfd   sp!,{r4-r12,pc}

// void* _activate_task(void* next_sp)
// returns final SP after _swi_handler is finished
.global _activate_task
_activate_task:
    // save the kernel's context
    stmfd   sp!,{r4-r12,lr}

    // Switch to system mode (IRQs disabled)
    msr     cpsr_c, #0xdf
    // move provided SP into sp
    mov     sp,r0

    // r0=syscall return value, r1=swi return address, r2=spsr
    ldmfd   sp!,{r0-r2}

    // restore user registers from stack
    add     sp,sp,#16 // skip popping r0 through r3
    ldmfd   sp!,{r4-r12,lr}

    // set the spsr to the user's saved spsr
    msr     spsr,r2
    movs    pc,r1

// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0363g/Chdhgibd.html
.global _enable_caches
_enable_caches:
    mrc     p15, 0, r1, c1, c0, 0  // read config register
    orr     r1, r1, #0x1 << 12     // enable instruction cache
    orr     r1, r1, #0x1 << 2      // enable data cache

    mcr     p15, 0, r0, c15, c5, 0 // Invalidate entire data cache
    mcr     p15, 0, r0, c7, c5, 0  // Invalidate entire instruction cache
    mcr     p15, 0, r1, c1, c0, 0  // enable caches

    bx      lr

// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0363g/Chdhgibd.html
.global _disable_caches
_disable_caches:
    mrc     p15, 0, r1, c1, c0, 0 // Read config register
    bic     r1, r1, #0x1 << 12    // instruction cache disable
    bic     r1, r1, #0x1 << 2     // data cache disable

    mcr     p15, 0, r1, c1, c0, 0  // disable caches

    bx      lr
