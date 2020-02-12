#include "common/printf.h"
#include "common/queue.h"
#include "common/ts7200.h"

#include "user/debug.h"
#include "user/tasks/uartserver.h"

#include <climits>
#include <cstdarg>

namespace Uart {
const char* SERVER_ID = "UartServer";
#define IOBUF_SIZE 4096

typedef Queue<char, IOBUF_SIZE> Iobuf;
static Iobuf com1_out;
static Iobuf com2_out;

Iobuf& outbuf(int channel) {
    switch (channel) {
        case COM1:
            return com1_out;
        case COM2:
            return com2_out;
        default:
            panic("outbuf: unknown channel %d", channel);
    }
}

struct Request {
    enum { Notify, Shutdown, Putc, Getc } tag;
    union {
        struct {
            int eventid;
            int data;
        } notify;
        struct {
        } shutdown;
        struct {
            int channel;
            char c;
        } putc;
        struct {
            int channel;
        } getc;
    };
};

struct Response {
    enum { Putc, Getc } tag;
    union {
        struct {
            bool success;
        } putc;
        struct {
            char c;
        } getc;
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

// void COM1Notifier() { Notifier(??); }
void COM2Notifier() { Notifier(54); }

void Server() {
    // TODO spawn the COM1 notifier too.
    //    Create(INT_MAX, COM1Notifier);
    Create(INT_MAX, COM2Notifier);

    RegisterAs(SERVER_ID);

    int tid;
    Request req;
    Response res;

    while (true) {
        int n = Receive(&tid, (char*)&req, sizeof(req));
        if (n != sizeof(req)) panic("Uart::Server: bad request length %d", n);
        switch (req.tag) {
            case Request::Notify: {
                break;
            }
            case Request::Putc: {
                int channel = req.putc.channel;
                // TODO extract this pls:
                volatile int *flags, *data;
                switch (channel) {
                    case COM1:
                        flags = (volatile int*)(UART1_BASE + UART_FLAG_OFFSET);
                        data = (volatile int*)(UART1_BASE + UART_DATA_OFFSET);
                        break;
                    case COM2:
                        flags = (volatile int*)(UART2_BASE + UART_FLAG_OFFSET);
                        data = (volatile int*)(UART2_BASE + UART_DATA_OFFSET);
                        break;
                    default:
                        panic("bad channel %d", channel);
                }

                Iobuf& buf = outbuf(req.putc.channel);

                while (!buf.is_empty() && !(*flags & TXFF_MASK)) {
                    char c = buf.pop_front().value();
                    *data = c;
                }

                res = {.tag = Response::Putc, .putc = {.success = false}};

                if (!(*flags & TXFF_MASK)) {
                    *data = req.putc.c;
                    res.putc.success = true;
                    Reply(tid, (char*)&res, sizeof(res));
                    break;
                }

                auto err = buf.push_back(req.putc.c);
                if (err == QueueErr::FULL) {
                    Reply(tid, (char*)&res, sizeof(res));
                    break;
                }
                // TODO we should enable TX interrupts on that uart here.
                res.putc.success = true;
                Reply(tid, (char*)&res, sizeof(res));
                break;
            }
            case Request::Getc: {
                panic("todo");
            }
            case Request::Shutdown: {
                panic("todo");
            }
        }
    }
}

void Shutdown(int tid) {
    Request req = {.tag = Request::Shutdown, .shutdown = {}};
    Send(tid, (char*)&req, sizeof(req), nullptr, 0);
}

int Getc(int tid, int channel) {
    (void)tid;
    (void)channel;

    panic("todo");
}

int Putc(int tid, int channel, char c) {
    Request req = {.tag = Request::Putc, .putc = {.channel = channel, .c = c}};
    Response res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    assert(n == sizeof(res));
    assert(res.tag == Response::Putc);
    if (res.putc.success) {
        return 1;
    }
    return -1;
}

struct _putc_arg {
    int tid;
    int channel;
};

static void _putc(char c, void* arg_vp) {
    _putc_arg* arg = (_putc_arg*)arg_vp;
    int n = Putc(arg->tid, arg->channel, c);
    if (n < 0) {
        panic("could not write character '0x%x' to channel %d", (unsigned int)c,
              arg->channel);
    }
}

int Printf(int tid, int channel, const char* format, ...) {
    _putc_arg args{.tid = tid, .channel = channel};
    va_list va;
    va_start(va, format);
    int n = vfctprintf(&_putc, &args, format, va);
    va_end(va);

    return n;
}

}  // namespace Uart
