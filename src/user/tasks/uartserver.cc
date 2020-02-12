#include "user/tasks/uartserver.h"
#include "user/debug.h"

namespace Uart {

struct Request {
    enum { Notify } tag;
    union {
        struct {
            int eventid;
            int data;
        } notify;
    };
};

static void Notifier(int eventid) {
    int myparent = MyParentTid();
    Request req = {.tag = Request::Notify,
                   .notify = {.eventid = eventid, .data = 0}};
    bool shutdown = false;
    while (!shutdown) {
        req.notify.data = AwaitEvent(eventid);
        int n = Send(myparent, (char*)&req, sizeof(req), (char*)&shutdown,
                     sizeof(shutdown));
        if (n != sizeof(shutdown))
            panic("Uart::Notifier - bad response length %d", n);
    }
}

void COM1Notifier() { Notifier(COM1); }
void COM2Notifier() { Notifier(COM2); }

void Server() {
    Create(0, COM1Notifier);
    Create(0, COM2Notifier);
}

int Getc(int tid, int channel) {
    (void)tid;
    (void)channel;

    panic("todo");
}

int Putc(int tid, int channel, char c) {
    (void)tid;
    (void)channel;
    (void)c;
    panic("todo");
}

int Printf(int tid, int channel, const char* format, ...) {
    (void)tid;
    (void)channel;
    (void)format;

    panic("todo");
}

}  // namespace Uart
