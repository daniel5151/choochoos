#include <stdbool.h>

#include <bwio.h>
#include <ts7200.h>

class PreMainTest {
   public:
    PreMainTest() {
        bwsetfifo(COM2, false);
        bwprintf(COM2, "Hello from before main!\r\n");
    };
};

PreMainTest test;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    bwprintf(COM2, "Hello from main!\r\n");

    return 1;
}
