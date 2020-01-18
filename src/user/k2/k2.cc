#include "bwio.h"
#include "user/syscalls.h"

void PongTask() {
    int tid;
    char msg[101] = {'\0'};
    Receive(&tid, msg, 100);
    bwprintf(COM2, "PongTask recieved msg '%s'\r\n", msg);
    Reply(tid, "pong", 4);
}

void FirstUserTask() {
    int pong_tid = Create(1, PongTask);
    char resp[100] = {'\0'};
    Send(pong_tid, "ping", 4, resp, 100);
    bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
}
