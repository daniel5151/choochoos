#include <ctype.h>
#include <stddef.h>
#include <string.h>

typedef void (*funcvoid0_t)();

// Defined in Linker Script
extern char __BSS_START__, __BSS_END__;
extern funcvoid0_t __INIT_ARRAY_START__, __INIT_ARRAY_END__;

// forward-declaration of main
int main(int argc, char* argv[]);

int __start() {
    // Zero out .bss
    memset(&__BSS_START__, 0, &__BSS_END__ - &__BSS_START__);

    // Run C++ global constructors
    for (funcvoid0_t* ctr = &__INIT_ARRAY_START__; ctr < &__INIT_ARRAY_END__;
         ctr += 1)
        (*ctr)();

    // Invoke main
    // TODO? see if redboot supports passing parameters?
    return main(0, 0);
}
