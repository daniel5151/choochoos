#include <cstddef>

#include "common/bwio.h"
#include "common/priority_queue.h"
#include "common/ts7200.h"
#include "kernel/asm.h"
#include "kernel/kernel.h"

extern int kmain();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    return kernel::run();
}
