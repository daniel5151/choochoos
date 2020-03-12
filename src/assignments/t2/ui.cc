#include "ui.h"

#include <cstring>  // memset

#include "common/bwio.h"
#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

constexpr int MAX_TRAINS = 2;

constexpr const char* SYS_PERF_TASK_NAME = "SYS_PERF_TASK";
constexpr char BLOCK_CHARS[6][4] = {"▁", "▂", "▃", "▅", "▆", "▇"};

/// Expects to receive a term_size_t upon startup
void SysPerfTask() {
    // ensure two copies of self haven't been spawned
    if (WhoIs(SYS_PERF_TASK_NAME) >= 0) {
        panic("tried to spawn two copies of SysPerfTask");
    }

    // register self with nameserver
    int nsres = RegisterAs(SYS_PERF_TASK_NAME);
    assert(nsres >= 0);

    // get configuration
    int tid;
    term_size_t term_size;
    int n = Receive(&tid, (char*)&term_size, sizeof(term_size));
    assert(n == sizeof(term_size));
    Reply(tid, nullptr, 0);

    // ensure clock and UART servers exist
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);
    assert(clock >= 0);
    assert(uart >= 0);

    // setup perf and output buffers
    uint8_t past_perf[term_size.width - 16];
    char outbuf[term_size.width * 3];

    memset(past_perf, 0, sizeof(past_perf));
    memset(outbuf, 0, sizeof(outbuf));

    perf_t perf;
    for (;;) {
        Perf(&perf);

        memmove(past_perf, past_perf + 1, sizeof(past_perf) - 1);
        past_perf[sizeof(past_perf) - 1] =
            (uint8_t)((100 - perf.idle_time_pct) / 18);

        size_t i = 0;
        for (uint8_t p : past_perf) {
            i += sprintf(outbuf + i, "%s", BLOCK_CHARS[p]);
        }

        Uart::Printf(uart, COM2,
                     VT_SAVE VT_TOPLEFT VT_HIDECUR
                     "CPU Usage (%02lu%%) %s" VT_SHOWCUR VT_RESTORE,
                     (100 - perf.idle_time_pct), outbuf);

        Clock::Delay(clock, (int)100);
    }
}

/// Creates a new Ui instance.
///
/// Panics if the UART server hasn't been instantiated.
Ui::Ui() {
    this->uart = WhoIs(Uart::SERVER_ID);
    assert(this->uart >= 0);
    this->clock = WhoIs(Clock::SERVER_ID);
    assert(this->clock >= 0);

    auto getc = [](void* d) {
        const int uart = *(int*)d;
        return (char)Uart::Getc(uart, COM2);
    };
    auto putc = [](void* d, const char* s) {
        const int uart = *(int*)d;
        Uart::Putstr(uart, COM2, s);
    };

    // read the term's dimensions
    Uart::Drain(uart, COM2);
    bool ok = query_term_size(&this->term_size, &this->uart, getc, putc);
    if (!ok) {
        panic("could not read terminal dimensions");
    };

    // check if the sysperf task needs to be spawned
    if (WhoIs(SYS_PERF_TASK_NAME) < 0) {
        int tid = Create(0, SysPerfTask);
        Send(tid, (char*)&this->term_size, sizeof(this->term_size), nullptr, 0);
    }
}

/// Paints all static elements onto the screen
void Ui::render_initial_screen() const {
    // clear the terminal
    Uart::Putstr(uart, COM2, VT_CLEAR);

    // render the prompt-separators, and set the scroll region
    char sep[term_size.width + 1];
    memset(sep, '-', term_size.width);
    sep[term_size.width] = '\0';

    int scroll_start = MAX_TRAINS + 5;
    int scroll_end = term_size.height;
    int sep_row = scroll_start - 1;

    Uart::Printf(uart, COM2,
                 VT_CLEAR VT_SET_SCROLL_FMT VT_ROWCOL_FMT
                 "%s" ENDL VT_ROWCOL_FMT "%s" ENDL VT_ROWCOL_FMT,
                 scroll_start, scroll_end, sep_row, 1, sep, sep_row - 2, 1, sep,
                 term_size.height, 1);
}

/// Render the train descriptor onto the screen
void Ui::render_train_descriptor(const train_descriptor_t& td) const {
    char line[256] = {0};

    int row = 3;  // TODO when rendering multiple trains, make this variable
    int col = 1;

    size_t n = snprintf(
        line, sizeof(line),
        VT_SAVE VT_ROWCOL_FMT VT_CLEARLN
        "[%5d] train=%2hhu spd=%2d vel=%4dmm/s pos=%c%02hhu ofst=%3dmm",
        row, col, td.pos_observed_at, td.id, td.speed, td.velocity,
        td.pos.sensor.group, td.pos.sensor.idx, td.pos.offset_mm);
    if (td.has_next_sensor) {
        auto sensor = td.next_sensor;
        int time = td.next_sensor_time;
        n += snprintf(line + n, sizeof(line) - n, " next=(%c%02hhu at t=%5d)",
                      sensor.group, sensor.idx, time);
    }
    if (td.has_error) {
        char sign = td.time_error < 0 ? '-' : '+';
        int error_sec = std::abs(td.time_error) / Clock::TICKS_PER_SEC;
        int error_dec = std::abs(td.time_error) % Clock::TICKS_PER_SEC;

        n += snprintf(line + n, sizeof(line) - n, " error=%c%d.%02ds/%c%dmm",
                      sign, error_sec, error_dec, sign,
                      std::abs(td.distance_error));
    }
    n += snprintf(line + n, sizeof(line) - n, ENDL VT_RESTORE);

    Uart::Putstr(uart, COM2, line);
}

/// Paint the command prompt, and wait for user to enter a command
void Ui::prompt_user(char* buf, size_t len) const {
    Uart::Drain(uart, COM2);  // clear any input entered while prompt was busy
    Uart::Putstr(uart, COM2, VT_ROWCOL(5, 1) VT_CLEARLN "> ");
    Uart::Getline(uart, COM2, buf, len);
    Uart::Printf(uart, COM2, VT_ROWCOL(5, 1) VT_CLEARLN "Processing...");
}

void Ui::shutdown() const {
    Uart::Putstr(uart, COM2, VT_RESET);
    Uart::Flush(uart, COM2);
}
