#include <cstddef>

#include "bwio.h"
#include "kernel/kernel.h"
#include "priority_queue.h"
#include "ts7200.h"

extern int kmain();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // hardware init
    bwsetfifo(COM2, false);

    return kmain();
}
