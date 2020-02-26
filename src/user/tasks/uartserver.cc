#include "user/tasks/uartserver.h"

#include <climits>
#include <cstdarg>
#include <cstring>
#include <optional>

#include "common/queue.h"
#include "common/ts7200.h"
#include "user/debug.h"
#include "user/tasks/clockserver.h"

namespace Uart {
const char* SERVER_ID = "UartServer";
#define IOBUF_SIZE 4096
#define MAX_GETN_SIZE 10
#define COM1_WAITING_FOR_DOWN_TIMEOUT 25  // 250ms

using Iobuf = Queue<char, IOBUF_SIZE>;
static Iobuf com1_out;
static Iobuf com2_out;

struct CTSState {
    enum {
        WAITING_FOR_DOWN = 'd',
        WAITING_FOR_UP = 'u',
        ACTUALLY_CTS = 'c'
    } tag;
    union {
        struct {
            int since;
        } waiting_for_down;
        struct {
        } waiting_for_up;
        struct {
        } actually_cts;
    };
};

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
    enum { Notify, Putstr, Getn, Drain, Flush } tag;
    union {
        struct {
            int channel;
            int eventid;
            UARTIntIDIntClr data;
        } notify;
        struct {
            int channel;
            size_t len;
            // must be the last element in this struct, may not be
            // completely copied.
            char buf[IOBUF_SIZE];
        } putstr;
        struct {
            int channel;
            size_t n;
        } getn;
        struct {
            int channel;
        } drain;
        struct {
            int channel;
        } flush;
    };
};

// Request.putstr.buf is very large, and it would be wasteful to copy the
// whole buf every time we want to write a string. Instead, tasks that send
// a Putstr request can truncate the request, only sending the tag, headers
// and first `len` bytes of `buf`. This only works because `buf` is the last
// member of Request.
static inline size_t putstr_request_size(const Request& req) {
    return offsetof(Request, putstr.buf) + req.putstr.len;
}

struct Response {
    enum { Putstr, Getn } tag;
    union {
        struct {
            bool success;
            int bytes_written;
        } putstr;
        struct {
            bool success;
            size_t n;
            char bytes[MAX_GETN_SIZE];
        } getn;
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

        // TODO sizeof(req) will be really big because of the Putstr buf we
        // should have a generic sizeof function that switches on the tag.
        debug("Notifier: received channel=%d data=0x%lx", channel,
              req.notify.data.raw);
        int n = Send(myparent, (char*)&req, sizeof(req), (char*)&shutdown,
                     sizeof(shutdown));
        if (n != sizeof(shutdown))
            panic("Uart::Notifier - bad response length %d", n);
    }
}

static void set_up_uarts() {
    // uart 2
    bwsetfifo(COM2, true);

    // uart 1
    volatile int* mid = (volatile int*)(UART1_BASE + UART_LCRM_OFFSET);
    volatile int* low = (volatile int*)(UART1_BASE + UART_LCRL_OFFSET);
    volatile int* high = (volatile int*)(UART1_BASE + UART_LCRH_OFFSET);

    *mid = 0x0;
    *low = 0xbf;

    int buf = *high;
    buf = (buf | STP2_MASK) & ~FEN_MASK;
    *high = buf;
}

void COM1Notifier() { Notifier(COM1, 52); }
void COM2Notifier() { Notifier(COM2, 54); }

static const volatile uint32_t* flags_for(int channel) {
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

    switch (channel) {
        case COM1:
            uart_ctlr._.enable_int_modem = 1;
            break;
        case COM2:
            uart_ctlr._.enable_int_tx = 1;
            break;
        default:
            panic("bad channel %d", channel);
    }
    debug("enable_tx_interrupts: channel=%d new_ctlr=0x%lx old_ctlr=0x%lx",
          channel, uart_ctlr.raw, old_ctlr);
    (void)old_ctlr;
    *ctlr = uart_ctlr.raw;
}

static bool clear_to_send(int channel, CTSState& com1_cts, int clock) {
    const volatile uint32_t* flags = flags_for(channel);
    switch (channel) {
        case COM1: {
            bool tx = !(*flags & TXFF_MASK);
            bool cts = (*flags & CTS_MASK);
            if (!(tx && cts)) return false;
            switch (com1_cts.tag) {
                case CTSState::ACTUALLY_CTS:
                    com1_cts = {
                        .tag = CTSState::WAITING_FOR_DOWN,
                        .waiting_for_down = {.since = Clock::Time(clock)}};
                    return true;
                case CTSState::WAITING_FOR_DOWN:
                    if (Clock::Time(clock) >
                        com1_cts.waiting_for_down.since +
                            COM1_WAITING_FOR_DOWN_TIMEOUT) {
                        com1_cts = {
                            .tag = CTSState::WAITING_FOR_DOWN,
                            .waiting_for_down = {.since = Clock::Time(clock)}};
                        return true;
                    }
                    return false;
                case CTSState::WAITING_FOR_UP:
                    return false;
            }
            return false;
        }
        case COM2:
            return !(*flags & TXFF_MASK);
        default:
            panic("bad channel %d", channel);
    }
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

struct rx_blocked_task_t {
    int tid;
    size_t written;
    Response getn_res;
};

struct flush_blocked_task_t {
    int tid;
    size_t bytes_remaining;
};

inline static void record_byte_sent(std::optional<flush_blocked_task_t>& t) {
    if (t.has_value()) {
        flush_blocked_task_t& task = t.value();
        task.bytes_remaining--;
        if (task.bytes_remaining == 0) {
            Reply(task.tid, nullptr, 0);
            t = std::nullopt;
        }
    }
}

void Server() {
    set_up_uarts();

    debug("Uart::Server: started");

    Create(INT_MAX, COM1Notifier);
    Create(INT_MAX, COM2Notifier);

    RegisterAs(SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);
    assert(clock >= 0);

    CTSState com1_cts = {.tag = CTSState::ACTUALLY_CTS, .actually_cts = {}};
    int tid;
    Request req;
    Response res;
    memset((char*)&res, 0, sizeof(res));

    // one for each channel
    std::optional<rx_blocked_task_t> rx_blocked_tids[2] = {std::nullopt};
    std::optional<flush_blocked_task_t> flush_blocked_tids[2] = {std::nullopt};

    while (true) {
        int reqlen = Receive(&tid, (char*)&req, sizeof(req));
        if (reqlen <= (int)sizeof(req.tag))
            panic("Uart::Server: bad request length %d", reqlen);
        switch (req.tag) {
            case Request::Notify: {
                // Reply to the notifier so it can start to AwaitEvent()
                // again.
                {
                    bool shutdown = false;
                    debug("Uart::Server: replying to notifier (tid %d)", tid);
                    Reply(tid, (char*)&shutdown, sizeof(shutdown));
                }

                int channel = req.notify.channel;
                Iobuf& buf = outbuf(channel);
                const volatile uint32_t* flags = flags_for(channel);
                volatile char* data = data_for(channel);

                debug("Server: received notify: channel=%d data=0x%lx", channel,
                      req.notify.data.raw);

                // TX
                if (req.notify.data._.tx || req.notify.data._.modem) {
                    if (channel == COM1 && req.notify.data._.modem) {
                        // only change to WAITING_FOR_UP once we observe it
                        // down
                        if ((*flags & CTS_MASK)) {
                            if (com1_cts.tag == CTSState::WAITING_FOR_UP)
                                com1_cts = {.tag = CTSState::ACTUALLY_CTS,
                                            .actually_cts = {}};
                        } else {
                            // cts went down, so now we wait for up
                            com1_cts = {.tag = CTSState::WAITING_FOR_UP,
                                        .waiting_for_up = {}};
                        }
                    }

                    int bytes_written = 0;
                    while (!buf.is_empty() &&
                           clear_to_send(channel, com1_cts, clock)) {
                        char c = buf.pop_front().value();
                        *data = (uint32_t)c;
                        if (channel == COM1) bwputc(COM2, 't');
                        record_byte_sent(flush_blocked_tids[channel]);
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

                // RX
                if (rx_blocked_tids[channel].has_value() &&
                    (req.notify.data._.rx || req.notify.data._.rx_timeout)) {
                    rx_blocked_task_t& blocked =
                        rx_blocked_tids[channel].value();
                    while (blocked.written < blocked.getn_res.getn.n &&
                           !(*flags & RXFE_MASK)) {
                        char c = *data;
                        blocked.getn_res.getn.bytes[blocked.written++] = c;
                    }

                    if (blocked.written == blocked.getn_res.getn.n) {
                        debug(
                            "replying to getn after interrupt tid=%d "
                            "channel=%d "
                            "n=%u bytes[0]=%02x bytes=%-10s" ENDL,
                            blocked.tid, channel, blocked.getn_res.getn.n,
                            blocked.getn_res.getn.bytes[0],
                            blocked.getn_res.getn.bytes);
                        Reply(blocked.tid, (char*)&blocked.getn_res,
                              sizeof(blocked.getn_res));
                        rx_blocked_tids[channel] = std::nullopt;
                    } else {
                        enable_rx_interrupts(channel);
                    }
                }
                break;
            }
            case Request::Putstr: {
                const int len = (int)req.putstr.len;
                int i = 0;
                int channel = req.putstr.channel;
                char* msg = req.putstr.buf;

                Iobuf& buf = outbuf(channel);
                volatile char* data = data_for(channel);

                if (buf.is_empty()) {
                    assert(!flush_blocked_tids[channel].has_value());

                    // try writing directly to the wire
                    for (; i < len && clear_to_send(channel, com1_cts, clock);
                         i++) {
                        *data = msg[i];
                        if (channel == COM1) bwputc(COM2, 'T');
                    }

                    if (i < len) {
                        // we couldn't write the whole message, so buffer
                        // the rest and enable interrupts.
                        for (; i < len; i++) {
                            auto err = buf.push_back(msg[i]);
                            if (err == QueueErr::FULL) {
                                panic(
                                    "Uart::Server: output buffer full for "
                                    "channel "
                                    "%d "
                                    "(trying to accept %d-byte write from "
                                    "tid "
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
                                "Uart::Server: output buffer full for "
                                "channel "
                                "%d "
                                "(trying to accept %d-byte write from tid "
                                "%d)",
                                channel, len, tid);
                        }
                    }
                }

                res = {.tag = Response::Putstr,
                       .putstr = {.success = true, .bytes_written = len}};
                int ret = Reply(tid, (char*)&res, sizeof(res));
                assert(ret >= 0);
                break;
            }
            case Request::Getn: {
                int channel = req.getn.channel;
                size_t n = req.getn.n;
                assert(n > 0);
                assert(n <= MAX_GETN_SIZE);
                const volatile uint32_t* flags = flags_for(channel);
                volatile char* data = data_for(channel);

                res = {.tag = Response::Getn,
                       .getn = {.success = false, .n = n, .bytes = {}}};

                debug("received Getn from tid %d channel=%d n=%u" ENDL, tid,
                      channel, n);
                if (rx_blocked_tids[channel].has_value()) {
                    panic(
                        "multiple tids trying to read from channel %d (%d "
                        "and "
                        "%d)",
                        channel, rx_blocked_tids[channel].value().tid, tid);
                    Reply(tid, (char*)&res, sizeof(res));
                    break;
                }

                res.getn.success = true;
                rx_blocked_tids[channel] = {
                    .tid = tid, .written = 0, .getn_res = res};
                rx_blocked_task_t& blocked = rx_blocked_tids[channel].value();

                while (!(*flags & RXFE_MASK) && blocked.written < n) {
                    char c = (char)*data;
                    blocked.getn_res.getn.bytes[blocked.written++] = c;
                }

                if (blocked.written == n) {
                    debug(
                        "replying to getn immediately tid=%d channel=%d "
                        "n=%u bytes[0]=%02x bytes=%-10s" ENDL,
                        tid, channel, n, blocked.getn_res.getn.bytes[0],
                        blocked.getn_res.getn.bytes);
                    Reply(tid, (char*)&blocked.getn_res,
                          sizeof(blocked.getn_res));
                    rx_blocked_tids[channel] = std::nullopt;
                    break;
                }

                enable_rx_interrupts(channel);
                break;
            }
            case Request::Drain: {
                int channel = req.drain.channel;
                const volatile uint32_t* flags = flags_for(channel);
                volatile char* data = data_for(channel);

                while (!(*flags & RXFE_MASK)) {
                    *data;
                }

                // If any task was blocked on a Getn with an incomplete
                // response, clear the response.
                if (rx_blocked_tids[channel].has_value()) {
                    rx_blocked_task_t& blocked =
                        rx_blocked_tids[channel].value();
                    blocked.written = 0;
                }

                Reply(tid, nullptr, 0);
                break;
            }
            case Request::Flush: {
                int channel = req.flush.channel;
                Iobuf& buf = outbuf(channel);
                if (buf.is_empty()) {
                    Reply(tid, nullptr, 0);
                    break;
                }
                if (flush_blocked_tids[channel].has_value()) {
                    panic("concurrent Flush unsupported (channel=%d)", channel);
                }
                flush_blocked_tids[channel] = {.tid = tid,
                                               .bytes_remaining = buf.size()};

                break;
            }
        }
    }
}  // namespace Uart

int Getc(int tid, int channel) {
    Request req = {.tag = Request::Getn, .getn = {.channel = channel, .n = 1}};
    Response res;
    int n = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    assert(n == sizeof(res));
    assert(res.tag == Response::Getn);
    if (res.getn.success) {
        return (int)res.getn.bytes[0];
    }
    return -1;
}

int Getn(int tid, int channel, size_t n, char* buf) {
    Request req = {.tag = Request::Getn, .getn = {.channel = channel, .n = n}};
    Response res;
    int ret = Send(tid, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    assert(ret == sizeof(res));
    assert(res.tag == Response::Getn);
    if (res.getn.success) {
        memcpy(buf, res.getn.bytes, n);
        return (int)n;
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

void Drain(int tid, int channel) {
    Request req = {.tag = Request::Drain, .drain = {.channel = channel}};
    Send(tid, (char*)&req, sizeof(req), nullptr, 0);
}

void Flush(int tid, int channel) {
    Request req = {.tag = Request::Flush, .flush = {.channel = channel}};
    Send(tid, (char*)&req, sizeof(req), nullptr, 0);
}

}  // namespace Uart
