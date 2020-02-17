#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "cmd.h"
#include "trainctl.h"

struct TermSize {
    size_t width;
    size_t height;
};

/// Use VT100 escape sequences to query the terminal's size.
static TermSize query_term_size(int uart) {
    // set the cursor to some arbitrarily large row/col, such that the response
    // returns the actual dimensions of the terminal.
    Uart::Putstr(uart, COM2, VT_SAVE VT_ROWCOL(999, 999) VT_GETPOS VT_RESTORE);

    // read VT_GETPOS response
    char vt_response[32];

    char c = '\0';
    size_t i = 0;
    while (c != 'R') {
        c = (char)Uart::Getc(uart, COM2);
        vt_response[i++] = c;
    }

    // extract dimensions from the response
    TermSize ret;
    int matches = sscanf(vt_response, "\033[%u;%uR", &ret.height, &ret.width);
    assert(matches == 2);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////

void TimerTask() {
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);

    assert(clock >= 0);
    assert(uart >= 0);

    while (true) {
        int ticks = Clock::Time(clock);
        if (ticks < 0) return;

        Uart::Printf(uart, COM2,
                     VT_SAVE VT_ROWCOL(1, 1) "[%d:%02d:%d]" VT_RESTORE,
                     ticks / (100 * 60), (ticks / 100) % 60, (ticks / 10) % 10);
        Clock::Delay(clock, 10);
    }
}

void LoggerTask() {
    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);
    int entry = 0;
    const int start_row = 3;
    const int lines = 10;

    assert(clock >= 0);
    assert(uart >= 0);

    srand(0);

    while (true) {
        for (int i = 0; i < lines; i++) {
            int row = start_row + i;
            entry++;
            Uart::Printf(
                uart, COM2,
                VT_SAVE VT_ROWCOL_FMT
                "time=%d log entry %d %08x%08x%08x%08x" ENDL VT_RESTORE,
                row, 1, Clock::Time(clock), entry, rand(), rand(), rand(),
                rand());
            Clock::Delay(clock, 10);
        }
    }
}

struct CmdTaskCfg {
    TermSize term_size;
};

void CmdTask() {
    int tid;
    CmdTaskCfg cfg;
    int n = Receive(&tid, (char*)&cfg, sizeof(cfg));
    assert(n == sizeof(cfg));
    Reply(tid, nullptr, 0);

    int uart = WhoIs(Uart::SERVER_ID);

    assert(uart >= 0);

    // -2 to compensate for the "> "
    size_t width = std::min((size_t)80, cfg.term_size.width - 2);

    char prev_line[width];
    char line[width];
    while (true) {
        Uart::Putstr(uart, COM2, "> ");
        Uart::Getline(uart, COM2, line, sizeof(line));

        // empty command re-runs last input
        if (line[0] == '\0') {
            strcpy(line, prev_line);
        } else {
            memcpy(prev_line, line, sizeof(prev_line));
        }

        std::optional<Command> cmd_opt = Command::from_string(line);
        if (!cmd_opt.has_value()) {
            Uart::Printf(uart, COM2, VT_CLEARLN "Invalid Cmd: '%s'" ENDL, line);
        } else {
            Command& cmd = cmd_opt.value();
            switch (cmd.kind) {
                case Command::Q:
                    bwputstr(COM2, VT_RESET);
                    panic("TODO: gracefully shutdown k4 lol");
                case Command::GO:
                case Command::LIGHT:
                case Command::RV:
                case Command::STOP:
                case Command::SW:
                case Command::TR:
                    Uart::Printf(uart, COM2, VT_CLEARLN "unimplemented" ENDL);
                    break;
            }
        }
    }
}

struct PerfTaskCfg {
    TermSize term_size;
};

void PerfTask() {
    int tid;
    PerfTaskCfg cfg;
    int n = Receive(&tid, (char*)&cfg, sizeof(cfg));
    assert(n == sizeof(cfg));
    Reply(tid, nullptr, 0);

    int clock = WhoIs(Clock::SERVER_ID);
    int uart = WhoIs(Uart::SERVER_ID);

    assert(clock >= 0);
    assert(uart >= 0);

    perf_t perf;
    for (;;) {
        Perf(&perf);
        Uart::Printf(uart, COM2,
                     VT_SAVE VT_ROWCOL_FMT "[Idle Time %02lu%%]" VT_RESTORE, 1,
                     cfg.term_size.width - 14, perf.idle_time_pct);
        Clock::Delay(clock, (int)5);
    }
}

void FirstUserTask() {
    int clock = Create(1000, Clock::Server);
    int uart = Create(1000, Uart::Server);

    assert(clock >= 0);
    assert(uart >= 0);

    // read the term's dimensions
    TermSize term_size = query_term_size(uart);

    Uart::Putstr(uart, COM2, VT_CLEAR VT_SET_SCROLL(14, 20) VT_ROWCOL(20, 1));

    // spawn the perf task
    int perf_task_tid = Create(0, PerfTask);
    {
        PerfTaskCfg cfg = {.term_size = term_size};
        Send(perf_task_tid, (char*)&cfg, sizeof(cfg), nullptr, 0);
    }

    Create(0, TimerTask);
    Create(0, LoggerTask);

    Uart::Printf(
        uart, COM2,
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
        "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
        "aliquip ex ea commodo consequat. Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
        "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
        "culpa qui officia deserunt mollit anim id est laborum." ENDL);

    int cmd_task_tid = Create(0, CmdTask);
    {
        CmdTaskCfg cfg = {.term_size = term_size};
        Send(cmd_task_tid, (char*)&cfg, sizeof(cfg), nullptr, 0);
    }
}
