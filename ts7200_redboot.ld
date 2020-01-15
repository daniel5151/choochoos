ENTRY(_start)

MEMORY {
    /* reserve the bottom 0x01000000 of RAM for RedBoot */
    ram : ORIGIN = 0x01000000, LENGTH = (32M - 0x01000000)
}

__HEAP_SIZE__ = 1M;

SECTIONS {
    . = 0x01000000;

    .text : ALIGN(4)
    {
        __TEXT_START__ = .;
        *(.text*);
        *(.rodata*);
        *(.got);
        *(.got.plt);

        __INIT_ARRAY_START__ = .;
        *(.init_array*)
        __INIT_ARRAY_END__   = ALIGN(4);

        __TEXT_END__ = ALIGN(4);
    } > ram

    .data : ALIGN(4)
    {
        __DATA_START__ = .;
        *(.data*);
        __DATA_END__ = ALIGN(4);
    } > ram

    .bss : ALIGN(4)
    {
        __BSS_START__ = .;
        *(.bss*);
        __BSS_END__ = ALIGN(4);
        /* needed for certain newlib routines that (potentially) call _sbrk */
        end = __BSS_END__;
    } > ram

    /* align the heap to the nearest 0x10000, just to make section sizes saner */
    .heap (NOLOAD) : ALIGN(0x10000)
    {
         __HEAP_START__ = .;
        . = . + __HEAP_SIZE__;
        __HEAP_END__ = ALIGN(4);
    } > ram

    __RAM_END__ = ORIGIN(ram) + LENGTH(ram);
    __STACK_SIZE__ = __RAM_END__ - __HEAP_END__;

    /* NOTE: RedBoot seems to seart its stack from __STACK_END__ as well  */
    /* It's probably best to just re-use the SP RedBoot hands us... */
    .stack (NOLOAD) : ALIGN(4)
    {
        __STACK_START__ = .;
        . = . + __STACK_SIZE__;
        __STACK_END__ = ALIGN(4);
    } > ram

    /DISCARD/ :
    {
        *(.ARM.exidx*) /* index entries for section unwinding */
        *(.ARM.extab*) /* exception unwinding information */
    }
}
