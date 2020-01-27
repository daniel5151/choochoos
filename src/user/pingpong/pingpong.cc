#include <cstdio>
#include <cstring>
#include "bwio.h"
#include "user/syscalls.h"

void PongTask() {
    int tid;
    char msg[101] = {'\0'};
    char resp[110];

    Receive(&tid, msg, 100);

    snprintf(resp, 110, "pong(%s)", msg);

    Reply(tid, resp, strlen(resp));
}

void Echo() {
    int my_tid = MyTid();
    int tid;
    char buf[12];
    while (true) {
        // receive 12 bytes
        int n = Receive(&tid, buf, sizeof(buf));
        printf("[tid %d] Echo received %d bytes\r\n", my_tid, n);

        // reply with the first 8
        n = Reply(tid, buf, 8);
        printf("[tid %d] Echo sent %d bytes\r\n", my_tid, n);
    }
}

void FirstUserTask() {
    int pong_tid_1 = Create(3, PongTask);  // lower priority
    int pong_tid_2 = Create(5, PongTask);  // higher priority

    int echo_lo = Create(3, Echo);
    int echo_hi = Create(5, Echo);

    assert(echo_lo >= 0);
    assert(echo_hi >= 0);

    // TODO make this compile
    // sending to an invalid task returns -1
    assert(Send(100, nullptr, 0, nullptr, 0) == -1);

    char buf[16];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = (char)('a' + (char)i);
    }

    char res[16] = {'\0'};

    // sending to a higher priority task returns the number of bytes written
    // back.
    assert(Send(echo_hi, buf, sizeof(buf), res, sizeof(res)) == 8);
    assert(Send(echo_lo, buf, sizeof(buf), res, sizeof(res)) == 8);
    assert(strcmp(res, "abcdefgh") == 0);

    assert(Send(echo_hi, buf, sizeof(buf), res, 7) == 7);
    assert(Send(echo_lo, buf, sizeof(buf), res, 7) == 7);

    // send to the lower priority task first
    {
        char resp[100] = {'\0'};
        assert(Send(pong_tid_1, "ping1", 6, resp, sizeof(resp) - 1) >= 0);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }
    {
        char resp[100] = {'\0'};
        assert(Send(pong_tid_2, "ping2", 6, resp, sizeof(resp) - 1) >= 0);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }

    pong_tid_1 = Create(3, PongTask);  // lower priority
    pong_tid_2 = Create(5, PongTask);  // higher priority

    assert(pong_tid_1 >= 0);
    assert(pong_tid_2 >= 0);

    // send to the higher priority task first
    {
        char resp[100] = {'\0'};
        assert(Send(pong_tid_2, "ping2", 6, resp, sizeof(resp) - 1) >= 0);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }
    {
        char resp[100] = {'\0'};
        assert(Send(pong_tid_1, "ping1", 6, resp, sizeof(resp) - 1) >= 0);
        bwprintf(COM2, "FirstUserTask recieved reply '%s'\r\n", resp);
    }
}
