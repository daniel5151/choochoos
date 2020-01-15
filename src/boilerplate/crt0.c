#include <ctype.h>
#include <stddef.h>
#include <string.h>

typedef void (*ctr_fn)();

// Defined by the linker script
extern char __BSS_START__, __BSS_END__;
extern ctr_fn __INIT_ARRAY_START__, __INIT_ARRAY_END__;

// forward-declaration of main
int main(int argc, char* argv[]);

int _start() {
    // Zero out .bss
    memset(&__BSS_START__, 0, &__BSS_END__ - &__BSS_START__);

    // Run C++ global constructors
    for (ctr_fn* ctr = &__INIT_ARRAY_START__; ctr < &__INIT_ARRAY_END__; ctr++)
        (*ctr)();

    // Invoke main
    // TODO? see if redboot supports passing parameters?
    return main(0, 0);
}
