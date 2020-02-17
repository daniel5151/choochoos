#include "user/tasks/uartserver.h"

#include <climits>
#include <cstdarg>
#include <cstring>
#include <optional>

#include "common/queue.h"
#include "common/ts7200.h"
#include "user/debug.h"

namespace Uart {
const char* SERVER_ID = "UartServer";
#define IOBUF_SIZE 4096

using Iobuf = Queue<char, IOBUF_SIZE>;
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
            bool success;
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
    debug("Uart::Notifier: started with channel=%d eventid=%d" ENDL, channel,
          eventid);
    while (!shutdown) {
        debug("Notifier: AwaitEvent(%d)", eventid);
        req.notify.data.raw = (uint32_t)AwaitEvent(eventid);

        // TODO sizeof(req) will be really big because of the Putstr buf
        //  we should have a generic sizeof function that switches on the tag.
        debug("Notifier: received channel=%d data=0x%lx", channel,
              req.notify.data.raw);
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

static volatile char* data_for(int channel) {
    switch (channel) {
        case COM1:
            return (volatile char*)(UART1_BASE + UART_DATA_OFFSET);
        case COM2:
            return (volatile char*)(UART2_BASE + UART_DATA_OFFSET);
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

static void enable_tx_interrupts(int channel) {
    volatile uint32_t* ctlr = ctlr_for(channel);
    UARTCtrl uart_ctlr = {.raw = *ctlr};
    uint32_t old_ctlr = uart_ctlr.raw;
    uart_ctlr._.enable_int_tx = 1;
    debug("enable_tx_interrupts: channel=%d new_ctlr=0x%lx old_ctlr=0x%lx",
          channel, uart_ctlr.raw, old_ctlr);
    (void)old_ctlr;
    *ctlr = uart_ctlr.raw;
}

static void enable_rx_interrupts(int channel) {
    volatile uint32_t* ctlr = ctlr_for(channel);
    UARTCtrl uart_ctlr = {.raw = *ctlr};
    uart_ctlr._.enable_int_rx = 1;
    uart_ctlr._.enable_int_rx_timeout = 1;
    debug("enable_rx_interrupts: channel=%d new_ctlr=0x%lx", channel,
          uart_ctlr.raw);
    *ctlr = uart_ctlr.raw;
}

void Server() {
    bwsetfifo(COM1, false);
    bwsetfifo(COM2, true);

    debug("Uart::Server: started");
    // TODO spawn the COM1 notifier too.
    //    Create(INT_MAX, COM1Notifier);
    Create(INT_MAX, COM2Notifier);

    RegisterAs(SERVER_ID);

    int tid;
    Request req;
    Response res;
    memset((char*)&res, 0, sizeof(res));

    // one for each channel
    std::optional<int> blocked_tids[2] = {std::nullopt};

    while (true) {
        int n = Receive(&tid, (char*)&req, sizeof(req));
        if (n <= (int)sizeof(req.tag))
            panic("Uart::Server: bad request length %d", n);
        switch (req.tag) {
            case Request::Notify: {
                int channel = req.notify.channel;
                Iobuf& buf = outbuf(channel);
                volatile uint32_t* flags = flags_for(channel);
                volatile char* data = data_for(channel);

                debug("Server: received notify: channel=%d data=0x%lx", channel,
                      req.notify.data.raw);

                if (req.notify.data._.tx) {
                    int bytes_written = 0;
                    while (!buf.is_empty() && !(*flags & TXFF_MASK)) {
                        char c = buf.pop_front().value();
                        *data = (uint32_t)c;
                        bytes_written++;
                    }
                    debug(
                        "Server: Notify: received tx and wrote %d "
                        "bytes, %u "
                        "left in buffer",
                        bytes_written, buf.size());

                    if (!buf.is_empty()) {
                        enable_tx_interrupts(channel);
                    }
                }

                if (blocked_tids[channel].has_value() &&
                    (req.notify.data._.rx || req.notify.data._.rx_timeout) &&
                    !(*flags & RXFE_MASK)) {
                    char c = (char)*data;
                    res = {.tag = Response::Getc,
                           .getc = {.success = true, .c = c}};
                    Reply(blocked_tids[channel].value(), (char*)&res,
                          sizeof(res));
                    blocked_tids[channel] = std::nullopt;
                }

                // Reply to the notifier so it can start to AwaitEvent() again.
                bool shutdown = false;
                debug("Uart::Server: replying to notifier (tid %d)", tid);
                Reply(tid, (char*)&shutdown, sizeof(shutdown));
                break;
            }
            case Request::Putstr: {
                const int len = (int)req.putstr.len;
                int i = 0;
                int channel = req.putstr.channel;
                char* msg = req.putstr.buf;

                Iobuf& buf = outbuf(channel);
                volatile uint32_t* flags = flags_for(channel);
                volatile char* data = data_for(channel);

                if (buf.is_empty()) {
                    // try writing directly to the wire
                    for (; i < len && !(*flags & TXFF_MASK); i++) {
                        *data = msg[i];
                    }

                    if (i < len) {
                        // we couldn't write the whole message, so buffer the
                        // rest and enable interrupts.
                        for (; i < len; i++) {
                            auto err = buf.push_back(msg[i]);
                            if (err == QueueErr::FULL) {
                                panic(
                                    "Uart::Server: output buffer full for "
                                    "channel "
                                    "%d "
                                    "(trying to accept %d-byte write from tid "
                                    "%d)",
                                    channel, len, tid);
                            }
                        }

                        enable_tx_interrupts(channel);
                    }
                } else {
                    // buffer the entire message
                    for (; i < len; i++) {
                        auto err = buf.push_back(msg[i]);
                        if (err == QueueErr::FULL) {
                            panic(
                                "Uart::Server: output buffer full for channel "
                                "%d "
                                "(trying to accept %d-byte write from tid %d)",
                                channel, len, tid);
                        }
                    }
                }

                res = {.tag = Response::Putstr,
                       .putstr = {.success = true, .bytes_written = len}};
                Reply(tid, (char*)&res, sizeof(res));
                break;
            }
            case Request::Getc: {
                int channel = req.getc.channel;
                volatile uint32_t* flags = flags_for(channel);
                volatile char* data = data_for(channel);
                if (!(*flags & RXFE_MASK)) {
                    char c = (char)*data;
                    res = {.tag = Response::Getc,
                           .getc = {.success = true, .c = c}};
                    Reply(tid, (char*)&res, sizeof(res));
                    break;
                }

                if (blocked_tids[channel].has_value()) {
                    debug(
                        "multiple tids trying to read from channel %d (%d and "
                        "%d)",
                        channel, blocked_tids[channel].value(), tid);
                    res = {.tag = Response::Getc,
                           .getc = {.success = false, .c = (char)0}};
                    Reply(tid, (char*)&res, sizeof(res));
                    break;
                }

                // record the tid, don't reply yet
                blocked_tids[channel] = tid;
                enable_rx_interrupts(channel);
                break;
            }
            case Request::Shutdown: {
                panic("todo: Uart::Server Shutdown");
            }
        }
    }
}  // namespace Uart

void Shutdown(int tid) {
    Request req = {.tag = Request::Shutdown, .shutdown = {}};
    Send(tid, (char*)&req, sizeof(req), nullptr, 0);
}

int Getc(int tid, int channel) {
    Request req = {.tag = Request::Getc, .getc = {channel}};
    Response res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    assert(n == sizeof(res));
    assert(res.tag == Response::Getc);
    if (res.getc.success) {
        return (int)res.getc.c;
    }
    return -1;
}

static int send_putstr(int tid, const Request& req) {
    Response res;
    int size = (int)putstr_request_size(req);
    int n = Send(tid, (char*)&req, size, (char*)&res, sizeof(res));
    assert(n == sizeof(res));
    assert(res.tag == Response::Putstr);
    if (res.putstr.success) return res.putstr.bytes_written;
    return -1;
}

int Putstr(int tid, int channel, const char* msg) {
    size_t len = strlen(msg);
    assert(len < IOBUF_SIZE);
    // `req` can contain a buffer up to length IOBUF_SIZE, but we only copy
    // strlen(msg) bytes into the request.
    Request req = {.tag = Request::Putstr,
                   .putstr = {.channel = channel, .len = len, .buf = {}}};
    memcpy(req.putstr.buf, msg, len);
    return send_putstr(tid, req);
}

int Putc(int tid, int channel, char c) {
    Request req = {.tag = Request::Putstr,
                   .putstr = {.channel = channel, .len = 1, .buf = {}}};
    req.putstr.buf[0] = c;
    return send_putstr(tid, req);
}

int Printf(int tid, int channel, const char* format, ...) {
    Request req = {.tag = Request::Putstr,
                   .putstr = {.channel = channel, .len = 0, .buf = {}}};

    va_list va;
    va_start(va, format);
    int len = vsnprintf(req.putstr.buf, sizeof(req.putstr.buf), format, va);
    assert(len >= 0);
    va_end(va);

    req.putstr.len = (size_t)len;

    return send_putstr(tid, req);
}

void Getline(int tid, int channel, char* line, size_t len) {
    size_t i = 0;
    while (true) {
        char c = (char)Getc(tid, channel);
        if (c == '\r') break;
        if (c == '\b') {
            if (i == 0) continue;
            i--;
            Putstr(tid, channel, "\b \b");
            continue;
        }
        if (i == len - 1) {
            // out of space - output a "ding" sound and ask for another
            // character
            Putc(tid, channel, '\a');
            continue;
        }
        if (!isprint(c)) continue;
        line[i++] = c;
        Putc(tid, channel, c);
    }
    Putstr(tid, channel, "\r\n");
    line[i] = '\0';
}

}  // namespace Uart
