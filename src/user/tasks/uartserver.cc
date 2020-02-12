#include "common/queue.h"
#include "common/ts7200.h"

#include "user/debug.h"
#include "user/tasks/uartserver.h"

#include <climits>
#include <cstdarg>
#include <cstring>

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
    enum { Notify, Shutdown, Putc, Putstr, Getc } tag;
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
            size_t len;
            // must be the last element in this struct, may not be completely
            // copied.
            char buf[IOBUF_SIZE];
        } putstr;
        struct {
            int channel;
        } getc;
    };
};

struct Response {
    enum { Putc, Putstr, Getc } tag;
    union {
        struct {
            bool success;
        } putc;
        struct {
            bool success;
            int bytes_written;
        } putstr;
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

static volatile int* flags_for(int channel) {
    switch (channel) {
        case COM1:
            return (volatile int*)(UART1_BASE + UART_FLAG_OFFSET);
        case COM2:
            return (volatile int*)(UART2_BASE + UART_FLAG_OFFSET);
        default:
            panic("bad channel %d", channel);
    }
}

static volatile int* data_for(int channel) {
    switch (channel) {
        case COM1:
            return (volatile int*)(UART1_BASE + UART_DATA_OFFSET);
        case COM2:
            return (volatile int*)(UART2_BASE + UART_DATA_OFFSET);
        default:
            panic("bad channel %d", channel);
    }
}

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
        if (n <= (int)sizeof(req.tag))
            panic("Uart::Server: bad request length %d", n);
        switch (req.tag) {
            case Request::Notify: {
                break;
            }
            case Request::Putc: {
                int channel = req.putc.channel;
                Iobuf& buf = outbuf(req.putc.channel);

                volatile int* flags = flags_for(channel);
                volatile int* data = data_for(channel);

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
            case Request::Putstr: {
                const int len = (int)req.putstr.len;
                int n = len;
                int channel = req.putstr.channel;
                char* msg = req.putstr.buf;

                Iobuf& buf = outbuf(req.putc.channel);
                volatile int* flags = flags_for(channel);
                volatile int* data = data_for(channel);

                while (n > 0 && !(*flags & TXFF_MASK)) {
                    *data = *msg;
                    msg++;
                    n--;
                }

                while (n > 0) {
                    if (buf.push_back(*msg) != QueueErr::OK) {
                        res = {.tag = Response::Putstr,
                               .putstr = {.success = false,
                                          .bytes_written = (int)(len - n)}};
                        Reply(tid, (char*)&res, sizeof(res));
                        break;
                    }
                    msg++;
                };

                if (!buf.is_empty()) {
                    // TODO enable UART interrupts
                }

                res = {.tag = Response::Putstr,
                       .putstr = {.success = true, .bytes_written = len}};
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

int Putstr(int tid, int channel, const char* msg) {
    size_t len = strlen(msg);
    assert(len < IOBUF_SIZE);
    Response res;

    // req can contain a buffer up to length IOBUF_SIZE, but we only copy
    // strlen(msg) bytes into the request.
    Request req = {.tag = Request::Putstr,
                   .putstr = {.channel = channel, .len = len, .buf = {}}};
    memcpy(req.putstr.buf, msg, len);

    // Moreover, since req.putstr.buf is the last field in the struct, we can
    // truncate the struct that we send to the server (instead of sending the
    // full size buffer regarless of len).
    int size = (int)(((char*)&req.putstr.buf - (char*)&req) + len);

    Send(tid, (char*)&req, size, (char*)&res, sizeof(res));

    assert(res.tag == Response::Putstr);
    if (res.putstr.success) {
        return res.putstr.bytes_written;
    }
    return -1;
}

int Printf(int tid, int channel, const char* format, ...) {
    char buf[IOBUF_SIZE];
    va_list va;
    va_start(va, format);
    vsnprintf(buf, sizeof(buf), format, va);
    va_end(va);

    return Putstr(tid, channel, buf);
}

}  // namespace Uart
