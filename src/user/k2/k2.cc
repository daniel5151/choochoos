#include "bwio.h"
#include "user/syscalls.h"

void PongTask() {}

void FirstUserTask() {
    int pong_tid = Create(1, PongTask);
    bwprintf(COM2, "k2!\r\n");
    char resp[100] = {'\0'};
    Send(pong_tid, "hello", 5, resp, 100);
}
