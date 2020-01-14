#include <stdbool.h>

#include <bwio.h>
#include <ts7200.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    bwsetfifo(COM2, false);
    bwprintf(COM2, "Hello World!\r\n");
    return 1;
}
