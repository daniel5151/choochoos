#include "user/tasks/uartserver.h"

#include <climits>
#include <cstdarg>
#include <cstring>

#include "common/queue.h"
#include "common/ts7200.h"
#include "user/debug.h"

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
    enum { Notify, Shutdown, Putstr, Getc } tag;
    union {
        struct {
            int channel;
            int eventid;
            UARTIntIDIntClr data;
        } notify;
        struct {
        } shutdown;
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

// Request.putstr.buf is very large, and it would be wasteful to copy the whole
// buf every time we want to write a string. Instead, tasks that send a Putstr
// request can truncate the request, only sending the tag, headers and first
// `len` bytes of `buf`. This only works because `buf` is the last member of
// Request.
static inline size_t putstr_request_size(const Request& req) {
    return offsetof(Request, putstr.buf) + req.putstr.len;
}

struct Response {
    enum { Putstr, Getc } tag;
    union {
        struct {
            bool success;
            int bytes_written;
        } putstr;
        struct {
            char c;
        } getc;
    };
};

static void Notifier(int channel, int eventid) {
    int myparent = MyParentTid();
    Request req = {
        .tag = Request::Notify,
        .notify = {.channel = channel, .eventid = eventid, .data = {0}}};
    bool shutdown = false;
    while (!shutdown) {
        req.notify.data.raw = (uint32_t)AwaitEvent(eventid);

        // TODO sizeof(req) will be really big because of the Putstr buf
        //  we should have a generic sizeof function that switches on the tag.
        int n = Send(myparent, (char*)&req, sizeof(req), (char*)&shutdown,
                     sizeof(shutdown));
        if (n != sizeof(shutdown))
            panic("Uart::Notifier - bad response length %d", n);
    }
}

// void COM1Notifier() { Notifier(??); }
void COM2Notifier() { Notifier(COM2, 54); }

static volatile uint32_t* flags_for(int channel) {
    switch (channel) {
        case COM1:
            return (volatile uint32_t*)(UART1_BASE + UART_FLAG_OFFSET);
        case COM2:
            return (volatile uint32_t*)(UART2_BASE + UART_FLAG_OFFSET);
        default:
            panic("bad channel %d", channel);
    }
}

static volatile uint32_t* data_for(int channel) {
    switch (channel) {
        case COM1:
            return (volatile uint32_t*)(UART1_BASE + UART_DATA_OFFSET);
        case COM2:
            return (volatile uint32_t*)(UART2_BASE + UART_DATA_OFFSET);
        default:
            panic("bad channel %d", channel);
    }
}

static volatile uint32_t* ctlr_for(int channel) {
    switch (channel) {
        case COM1:
            return (volatile uint32_t*)(UART1_BASE + UART_CTLR_OFFSET);
        case COM2:
            return (volatile uint32_t*)(UART2_BASE + UART_CTLR_OFFSET);
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
                int channel = req.notify.channel;
                Iobuf& buf = outbuf(channel);
                volatile uint32_t* flags = flags_for(channel);
                volatile uint32_t* data = data_for(channel);

                if (req.notify.data._.tx) {
                    while (!buf.is_empty() && !(*flags & TXFF_MASK)) {
                        char c = buf.pop_front().value();
                        *data = (uint32_t)c;
                    }
                }

                bool shutdown = false;
                Reply(tid, (char*)&shutdown, sizeof(shutdown));
                break;
            }
            case Request::Putstr: {
                const int len = (int)req.putstr.len;
                int n = len;
                int channel = req.putstr.channel;
                char* msg = req.putstr.buf;

                Iobuf& buf = outbuf(channel);
                volatile uint32_t* flags = flags_for(channel);
                volatile uint32_t* data = data_for(channel);

                if (buf.is_empty()) {
                    for (; n > 0 && !(*flags & TXFF_MASK); n--) {
                        *data = (uint32_t)*msg;
                        msg++;
                    }
                }

                for (; n > 0; n--) {
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
                    // enable tx interrupts on the uart
                    volatile uint32_t* ctlr = ctlr_for(channel);
                    UARTCtrl uart_ctlr = {.raw = *ctlr};
                    uart_ctlr._.enable_int_tx = 1;
                    *ctlr = uart_ctlr.raw;
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

int Putstr(int tid, int channel, const char* msg) {
    size_t len = strlen(msg);
    assert(len < IOBUF_SIZE);
    Response res;
    // `req` can contain a buffer up to length IOBUF_SIZE, but we only copy
    // strlen(msg) bytes into the request.
    Request req = {.tag = Request::Putstr,
                   .putstr = {.channel = channel, .len = len, .buf = {}}};
    memcpy(req.putstr.buf, msg, len);
    int size = (int)putstr_request_size(req);
    Send(tid, (char*)&req, size, (char*)&res, sizeof(res));
    assert(res.tag == Response::Putstr);
    if (res.putstr.success) return res.putstr.bytes_written;
    return -1;
}

int Putc(int tid, int channel, char c) {
    Request req = {.tag = Request::Putstr,
                   .putstr = {.channel = channel, .len = 1, .buf = {}}};
    req.putstr.buf[0] = c;
    Response res;
    int size = (int)putstr_request_size(req);
    int n = Send(tid, (char*)&req, size, (char*)&res, sizeof(res));
    assert(n == sizeof(res));
    assert(res.tag == Response::Putstr);
    if (res.putstr.success) return 1;
    return -1;
}

int Printf(int tid, int channel, const char* format, ...) {
    Request req = {.tag = Request::Putstr,
                   .putstr = {.channel = channel, .len = 0, .buf = {}}};

    va_list va;
    va_start(va, format);
    int len = vsnprintf(req.putstr.buf, sizeof(req.putstr.buf), format, va);
    va_end(va);

    assert(len >= 0);
    Response res;
    req.putstr.len = (size_t)len;
    int size = (int)putstr_request_size(req);
    Send(tid, (char*)&req, size, (char*)&res, sizeof(res));
    assert(res.tag == Response::Putstr);
    if (res.putstr.success) return res.putstr.bytes_written;
    return -1;
}

}  // namespace Uart
