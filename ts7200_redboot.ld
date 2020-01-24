ENTRY(_start)

MEMORY {
    /* reserve the bottom 0x01000000 of RAM for RedBoot */
    ram : ORIGIN = 0x01000000, LENGTH = (32M - 0x01000000)
}

__HEAP_SIZE__ = 1M;
__KERNEL_STACK_SIZE__ = 512K;

SECTIONS {
    . = 0x01000000;

    .text : ALIGN(4)
    {
        __TEXT_START__ = .;
        *(.text*);
        . = ALIGN(4);
        __TEXT_END__ = .;

        __INIT_ARRAY_START__ = .;
        *(.init_array*)
        __INIT_ARRAY_END__ = .;
    } > ram

    .got : ALIGN(4)
    {
        *(.got*);
    } > ram

    .data : ALIGN(4)
    {
        __DATA_START__ = .;
        *(.data*);
        *(.rodata*);
        __DATA_END__ = .;
    } > ram

    .bss : ALIGN(4)
    {
        __BSS_START__ = .;
        *(.bss*);
        . = ALIGN(4);
        __BSS_END__ = .;
        /* needed for certain newlib routines that (potentially) call _sbrk */
        end = __BSS_END__;
    } > ram

    /* Block-out memory regions for the heap, kernel stack, and user stacks */

    /* This _could_ be done via magic numbers in the kernel code itself, but
       it's nice to leverage the linker's understanding of how things are layed
       out in memory to demarcate section boundaries */

    /* align the heap to the nearest 0x10000, just to make section sizes saner */
    . = ALIGN(1M);
    __HEAP_START__ = .;
    . = . + __HEAP_SIZE__;
    __HEAP_END__ = .;

    __RAM_END__ = ORIGIN(ram) + LENGTH(ram);
    __USER_STACKS_SIZE__ = __RAM_END__ - __HEAP_END__ - __KERNEL_STACK_SIZE__;

    /* NOTE: RedBoot seems to start its' stack from __STACK_END__ as well */
    /* It's probably best to just re-use the SP RedBoot hands us... */
    __USER_STACKS_START__ = .;
    . = . + __USER_STACKS_SIZE__;
    __USER_STACKS_END__ = .;

    __KERNEL_STACK_START__ = .;
    . = . + __KERNEL_STACK_SIZE__;
    __KERNEL_STACK_END__ = .;

    /DISCARD/ :
    {
        *(.ARM.exidx*) /* index entries for section unwinding */
        *(.ARM.extab*) /* exception unwinding information */
    }
}
