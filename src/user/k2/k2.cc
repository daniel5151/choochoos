#include "bwio.h"
#include "user/syscalls.h"

#include <cstdio>
#include <cstring>

void PongTask() {
    int tid;
    char msg[101] = {'\0'};
    char resp[110];

    Receive(&tid, msg, 100);

    snprintf(resp, 110, "pong(%s)", msg);

    Reply(tid, resp, strlen(resp));
}

void FirstUserTask() {
    int pong_tid_1 = Create(3, PongTask);  // lower priority
    int pong_tid_2 = Create(5, PongTask);  // higher priority

    // send to the lower priority task first
    {
        char resp[100] = {'\0'};
        Send(pong_tid_1, "ping1", 6, resp, 99);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }
    {
        char resp[100] = {'\0'};
        Send(pong_tid_2, "ping2", 6, resp, 99);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }

    pong_tid_1 = Create(3, PongTask);  // lower priority
    pong_tid_2 = Create(5, PongTask);  // higher priority

    // send to the higher priority task first
    {
        char resp[100] = {'\0'};
        Send(pong_tid_2, "ping2", 6, resp, 99);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }
    {
        char resp[100] = {'\0'};
        Send(pong_tid_1, "ping1", 6, resp, 99);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }
}
