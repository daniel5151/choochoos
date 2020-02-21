#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "common/queue.h"
#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "cmd.h"
#include "trainctl.h"

#define NUM_SENSOR_GROUPS 5

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

struct MarklinAction {
    enum { Go, Stop, Train, Switch, QuerySensors } tag;
    union {
        struct {
        } go;
        struct {
        } stop;
        struct {
            size_t no;
            TrainState state;
        } train;
        struct {
            size_t no;
            SwitchDir dir;
        } sw;
        struct {
        } query_sensors;
    };
};

void wait_for_sensor_response(int uart, char bytes[NUM_SENSOR_GROUPS * 2]) {
    int res = Uart::Getn(uart, COM1, NUM_SENSOR_GROUPS * 2, bytes);
    if (res < 0)
        panic("cannot read %d bytes from UART 1: %d", NUM_SENSOR_GROUPS * 2,
              res);
}

struct sensor_t {
    char group;
    uint8_t idx;
};

bool sensor_eq(const sensor_t& s1, const sensor_t& s2) {
    return s1.group == s2.group && s1.idx == s2.idx;
}

using SensorQueue = Queue<sensor_t, 10>;

size_t enqueue_sensors(const char bytes[NUM_SENSOR_GROUPS * 2],
                       SensorQueue& q) {
    size_t ret = 0;
    for (size_t bi = 0; bi < NUM_SENSOR_GROUPS * 2; bi++) {
        char byte = bytes[bi];
        for (size_t i = 1; i <= 8; i++) {
            if ((byte >> (8 - i)) & 0x01) {
                char group = (char)((int)'A' + (bi / 2));
                uint8_t idx = (uint8_t)(i + (8 * (bi % 2)));

                const sensor_t s = {.group = group, .idx = idx};

                // Don't push the sensor onto the queue if it is the most
                // recently triggered sensor.
                if (q.size() > 0 && sensor_eq(s, *q.peek_index(q.size() - 1))) {
                    continue;
                }

                if (q.available() == 0) q.pop_front();
                q.push_back(s);
                ret++;
            }
        }
    }

    return ret;
}

void report_sensor_values(SensorQueue& q,
                          int uart,
                          int time,
                          int roundtrip,
                          const char bytes[NUM_SENSOR_GROUPS * 2]) {
    size_t num_enqueued = enqueue_sensors(bytes, q);
    if (num_enqueued == 0) return;

    char line[256];

    int n = snprintf(
        line, sizeof(line),
        VT_SAVE VT_ROWCOL(2, 1) VT_CLEARLN "time=%d rtt=%d sensors: ", time,
        roundtrip);

    // TODO this is inconsisent on the track! Sometimes we're in a good state
    // and the sensor values are correct. Other times it appears as though the
    // first two bytes from the track are always zero, so our sensors values are
    // all wrong. Maybe the bug has to do with the Getn from COM1?
    for (int i = (int)q.size() - 1; i >= 0; i--) {
        const sensor_t* s = q.peek_index((size_t)i);
        assert(s != nullptr);
        n += snprintf(line + n, sizeof(line) - (size_t)n, "%c%02u ", s->group,
                      s->idx);
    }

    // useful for debugging:
    n += snprintf(line + n, sizeof(line) - (size_t)n, " raw: ");
    for (size_t i = 0; i < NUM_SENSOR_GROUPS * 2; i++) {
        char byte = bytes[i];
        n += snprintf(line + n, sizeof(line) - (size_t)n, "%02x ", byte);
    }

    snprintf(line + n, sizeof(line) - (size_t)n, VT_RESTORE);
    Uart::Putstr(uart, COM2, line);
}

void MarklinCommandTask() {
    SensorQueue sensor_queue;
    int uart = WhoIs(Uart::SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);

    assert(uart >= 0);
    assert(clock >= 0);

    int nsres = RegisterAs("MarklinCommandTask");
    assert(nsres >= 0);

    MarklinAction act;
    int tid;
    for (;;) {
        Receive(&tid, (char*)&act, sizeof(act));
        switch (act.tag) {
            case MarklinAction::Go:
                Uart::Putc(uart, COM1, 0x60);
                break;
            case MarklinAction::Stop:
                Uart::Putc(uart, COM1, 0x61);
                break;
            case MarklinAction::Train:
                Uart::Putc(uart, COM1, (char)act.train.state.raw);
                Uart::Putc(uart, COM1, (char)act.train.no);
                break;
            case MarklinAction::Switch:
                Uart::Putc(uart, COM1,
                           act.sw.dir == SwitchDir::Straight ? 0x21 : 0x22);
                Uart::Putc(uart, COM1, (char)act.sw.no);
                break;
            case MarklinAction::QuerySensors: {
                char bytes[NUM_SENSOR_GROUPS * 2] = {'\0'};
                int start = Clock::Time(clock);
                Uart::Putc(uart, COM1, (char)(128 + NUM_SENSOR_GROUPS));
                wait_for_sensor_response(uart, bytes);
                int end = Clock::Time(clock);
                report_sensor_values(sensor_queue, uart, end, end - start,
                                     bytes);
            } break;
            default:
                panic("MarklinCommandTask received an invalid action");
        }
        Reply(tid, nullptr, 0);

        // ensure that commands have at least 150ms of delay
        Clock::Delay(clock, (int)15);
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
    int clock = WhoIs(Clock::SERVER_ID);
    int marklin = WhoIs("MarklinCommandTask");

    assert(uart >= 0);
    assert(clock >= 0);
    assert(marklin >= 0);

    Uart::Printf(uart, COM2, "Ready." ENDL);

    // TODO: move train/track state to a dedicated UI/Marklin Controller
    // task possible approach: have the cmd task forward commands to the UI
    // task, which would have all the train/track state. That task would be
    // responsible for keeping the UI in-sync with whatever commands the
    // marklin box is sent.
    TrainState train[256] = {0};

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
                case Command::Q: {
                    bwputstr(COM2, VT_RESET);
                    panic("TODO: gracefully shutdown k4 lol");
                } break;
                case Command::GO: {
                    MarklinAction act = {.tag = MarklinAction::Go, .go = {}};
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    Uart::Printf(uart, COM2, VT_CLEARLN "Sent GO command" ENDL);
                } break;
                case Command::LIGHT: {
                    train[cmd.light.no]._.light = !train[cmd.light.no]._.light;

                    MarklinAction act = {
                        .tag = MarklinAction::Train,
                        .train = {.no = cmd.light.no,
                                  .state = train[cmd.light.no]}};
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    Uart::Printf(uart, COM2,
                                 VT_CLEARLN "Toggled lights on train %u" ENDL,
                                 cmd.light.no);
                } break;
                case Command::RV: {
                    MarklinAction act = {
                        .tag = MarklinAction::Train,
                        .train = {.no = cmd.rv.no, .state = train[cmd.rv.no]}};

                    // stop the train
                    act.train.state._.speed = 0;
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    // wait for train to slow down
                    Uart::Printf(uart, COM2, VT_CLEARLN "Stopping..." ENDL);
                    Clock::Delay(clock, (int)400);

                    // reverse the train
                    act.train.state._.speed = 15;
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    act.train.state._.speed = train[cmd.rv.no]._.speed;
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    Uart::Printf(uart, COM2,
                                 VT_CLEARLN "Reversed train %u" ENDL,
                                 cmd.rv.no);
                } break;
                case Command::STOP: {
                    MarklinAction act = {.tag = MarklinAction::Stop,
                                         .stop = {}};
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    Uart::Printf(uart, COM2,
                                 VT_CLEARLN "Sent STOP command" ENDL);
                } break;
                case Command::SW: {
                    MarklinAction act = {
                        .tag = MarklinAction::Switch,
                        .sw = {.no = cmd.sw.no, .dir = cmd.sw.dir}};
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    Uart::Printf(uart, COM2,
                                 VT_CLEARLN "Set switch %u to be %s" ENDL,
                                 cmd.sw.no,
                                 cmd.sw.dir == SwitchDir::Straight ? "straight"
                                                                   : "curved");
                } break;
                case Command::TR: {
                    if (cmd.tr.speed == 15) {
                        Uart::Putstr(
                            uart, COM2,
                            VT_CLEARLN
                            "please use rv command to reverse train" ENDL);
                        break;
                    }
                    train[cmd.tr.no]._.speed = (unsigned)cmd.tr.speed & 0x0f;

                    MarklinAction act = {
                        .tag = MarklinAction::Train,
                        .train = {.no = cmd.tr.no, .state = train[cmd.tr.no]}};
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    Uart::Printf(uart, COM2,
                                 VT_CLEARLN "Set train %u to speed %u" ENDL,
                                 cmd.tr.no, cmd.tr.speed);
                } break;
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

const size_t VALID_SWITCHES[] = {1,  2,  3,   4,   5,   6,  7,  8,
                                 9,  10, 11,  12,  13,  14, 15, 16,
                                 17, 18, 153, 154, 155, 156};
const size_t VALID_TRAINS[] = {1, 24, 58, 74, 78, 79};

// TODO: send train commands through UI/Controller task for visual feedback
void init_track(int marklin) {
    MarklinAction act;
    memset(&act, 0, sizeof(act));

    // stop all the trains
    act.tag = MarklinAction::Train;
    act.train.state._.speed = 0;
    act.train.state._.light = 0;
    for (size_t no : VALID_TRAINS) {
        act.train.no = no;
        Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
    }

    // set all the switches to curved
    act.tag = MarklinAction::Switch;
    act.sw.dir = SwitchDir::Curved;
    for (size_t no : VALID_SWITCHES) {
        act.sw.no = no;
        Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
    }

    // set innter-ring switches straight
    act.tag = MarklinAction::Switch;
    act.sw.dir = SwitchDir::Straight;
    for (size_t no : {10, 13, 17, 16}) {
        act.sw.no = no;
        Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
    }
}

void SensorPollerTask() {
    int marklin = WhoIs("MarklinCommandTask");
    int clock = WhoIs(Clock::SERVER_ID);

    assert(marklin >= 0);
    assert(clock >= 0);

    MarklinAction act = {.tag = MarklinAction::QuerySensors,
                         .query_sensors = {}};
    while (true) {
        Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
        Clock::Delay(clock, 30);
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

    int marklin = Create(1, MarklinCommandTask);
    (void)marklin;

    Uart::Printf(uart, COM2, "Initializing Track..." ENDL);
    init_track(marklin);

    Create(0, SensorPollerTask);

    int cmd_task_tid = Create(0, CmdTask);
    {
        CmdTaskCfg cfg = {.term_size = term_size};
        Send(cmd_task_tid, (char*)&cfg, sizeof(cfg), nullptr, 0);
    }
}
