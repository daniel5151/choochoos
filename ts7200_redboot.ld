ENTRY(__start)

MEMORY {
    /* reserve the bottom 0x01000000 of RAM for RedBoot */
    ram : ORIGIN = 0x01000000, LENGTH = (32M - 0x01000000)
}

__HEAP_SIZE__ = 1M;

SECTIONS {
    . = 0x01000000;

    .bss : ALIGN(4)
    {
        __BSS_START__ = .;
        *(.bss*);
        __BSS_END__ = ALIGN(4);
        /* needed for certain newlib routines that (potentially) call _sbrk */
        end = __BSS_END__;
    } > ram


    .data : ALIGN(4)
    {
        __DATA_START__ = .;
        *(.data*);
        __DATA_END__ = ALIGN(4);
    } > ram

    .text : ALIGN(4)
    {
        __TEXT_START__ = .;
        *(.text*);
        *(.rodata*);
        *(.got);
        *(.got.plt);
        __TEXT_END__ = ALIGN(4);
    } > ram

    .init_array : ALIGN(4)
    {
        __INIT_ARRAY_START__ = .;
        *(.init_array);
        __INIT_ARRAY_END__ = ALIGN(4);
    } > ram

    __HEAP_START__ = .;

    /DISCARD/ :
    {
        *(.ARM.exidx*) /* index entries for section unwinding */
        *(.ARM.extab*) /* exception unwinding information */
    }
}
