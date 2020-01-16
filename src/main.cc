#include <cstddef>

#include "kernel.h"
#include "priority_queue.h"
#include "syscalls.h"
#include "ts7200.h"

extern int kmain();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    bwsetfifo(COM2, false);
    bwprintf(COM2, "Hello from the kernel!\r\n");

    return kmain();
}
