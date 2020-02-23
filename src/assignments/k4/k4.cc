#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "common/queue.h"
#include "common/ts7200.h"
#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

#include "cmd.h"
#include "trainctl.h"
#include "ui.h"

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

struct sensor_t {
    char group;
    uint8_t idx;

    bool operator==(const sensor_t& other) const {
        return this->group == other.group && this->idx == other.idx;
    }
};

using SensorQueue = Queue<sensor_t, 10>;

static size_t enqueue_sensors(const char bytes[NUM_SENSOR_GROUPS * 2],
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
                if (q.size() > 0 && s == *q.peek_index(q.size() - 1)) {
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

static void report_sensor_values(SensorQueue& q,
                                 int uart,
                                 int time,
                                 const char bytes[NUM_SENSOR_GROUPS * 2]) {
    (void)time;
    size_t num_enqueued = enqueue_sensors(bytes, q);
    if (num_enqueued == 0 && q.size() > 0) return;

    char line[256];

    int n = snprintf(line, sizeof(line),
                     VT_SAVE VT_ROWCOL(3, 1) VT_CLEARLN "Sensors: ");

    for (int i = (int)q.size() - 1; i >= 0; i--) {
        const sensor_t* s = q.peek_index((size_t)i);
        assert(s != nullptr);
        n += snprintf(line + n, sizeof(line) - (size_t)n, "%c%02u ", s->group,
                      s->idx);
    }

    // useful for debugging:
    // n += snprintf(line + n, sizeof(line) - (size_t)n, " time=%d raw: ",
    // time); for (size_t i = 0; i < NUM_SENSOR_GROUPS * 2; i++) {
    //     char byte = bytes[i];
    //     n += snprintf(line + n, sizeof(line) - (size_t)n, "%02x ", byte);
    // }

    snprintf(line + n, sizeof(line) - (size_t)n, VT_RESTORE);
    Uart::Putstr(uart, COM2, line);
}

void SensorReporterTask() {
    SensorQueue sensor_queue;
    char bytes[NUM_SENSOR_GROUPS * 2] = {'\0'};

    int uart = WhoIs(Uart::SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);

    assert(uart >= 0);
    assert(clock >= 0);

    while (true) {
        int res = Uart::Getn(uart, COM1, NUM_SENSOR_GROUPS * 2, bytes);
        if (res < 0)
            panic("cannot read %d bytes from UART 1: %d", NUM_SENSOR_GROUPS * 2,
                  res);

        report_sensor_values(sensor_queue, uart, Clock::Time(clock), bytes);
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
                    Ui::render_train_speed(uart, (uint8_t)cmd.rv.no, 0);

                    // wait for train to slow down
                    Uart::Printf(uart, COM2, VT_CLEARLN "Stopping..." ENDL);
                    Clock::Delay(clock, (int)400);

                    // reverse the train
                    act.train.state._.speed = 15;
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    act.train.state._.speed = train[cmd.rv.no]._.speed;
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
                    Ui::render_train_speed(uart, (uint8_t)cmd.rv.no,
                                           act.train.state._.speed);

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
                    Ui::render_switch_direction(uart, (uint8_t)cmd.sw.no,
                                                cmd.sw.dir);

                    Uart::Printf(uart, COM2,
                                 VT_CLEARLN "Set switch %u to be %s" ENDL,
                                 cmd.sw.no,
                                 cmd.sw.dir == SwitchDir::Straight ? "straight"
                                                                   : "curved");
                } break;
                case Command::TR: {
                    if (cmd.tr.speed == 15) {
                        Uart::Putstr(
                            uart, COM2, VT_CLEARLN
                            "please use rv command to reverse train" ENDL);
                        break;
                    }
                    if (cmd.tr.speed >= 16) {
                        Uart::Putstr(
                            uart, COM2, VT_CLEARLN
                            "tr command only supports speeds 0-14" ENDL);
                        break;
                    }

                    train[cmd.tr.no]._.speed = (unsigned)cmd.tr.speed & 0x0f;

                    MarklinAction act = {
                        .tag = MarklinAction::Train,
                        .train = {.no = cmd.tr.no, .state = train[cmd.tr.no]}};
                    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

                    Ui::render_train_speed(uart, (uint8_t)cmd.tr.no,
                                           cmd.tr.speed);

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
        Clock::Delay(clock, (int)25);
    }
}

void init_track(int uart, int marklin) {
    MarklinAction act;
    memset(&act, 0, sizeof(act));

    // send a single sensor query
    act = {.tag = MarklinAction::QuerySensors, .query_sensors = {}};
    Send(marklin, (char*)&act, sizeof(act), nullptr, 0);

    // stop all the trains
    act.tag = MarklinAction::Train;
    act.train.state._.speed = 0;
    act.train.state._.light = 0;
    for (uint8_t no : VALID_TRAINS) {
        act.train.no = no;
        Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
        Ui::render_train_speed(uart, no, 0);
    }

    // set all the switches to curved
    act.tag = MarklinAction::Switch;
    act.sw.dir = SwitchDir::Curved;
    for (uint8_t no : VALID_SWITCHES) {
        act.sw.no = no;
        Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
        Ui::render_switch_direction(uart, no, SwitchDir::Curved);
    }

    // set inter-ring switches straight
    act.tag = MarklinAction::Switch;
    act.sw.dir = SwitchDir::Straight;
    for (size_t no : {10, 13, 17, 16}) {
        act.sw.no = no;
        Send(marklin, (char*)&act, sizeof(act), nullptr, 0);
        Ui::render_switch_direction(uart, (uint8_t)no, SwitchDir::Straight);
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
    Uart::Drain(uart, COM2);
    TermSize term_size = query_term_size(
        &uart, [](void* d) { return (char)Uart::Getc(*(int*)d, COM2); },
        [](void* d, const char* s) { Uart::Putstr(*(int*)d, COM2, s); });
    assert(term_size.success);

    Uart::Putstr(uart, COM2, VT_CLEAR VT_SET_SCROLL(14, 20) VT_ROWCOL(20, 1));

    // spawn the perf task
    int perf_task_tid = Create(0, PerfTask);
    {
        PerfTaskCfg cfg = {.term_size = term_size};
        Send(perf_task_tid, (char*)&cfg, sizeof(cfg), nullptr, 0);
    }

    Create(0, TimerTask);

    Ui::render_initial_screen(uart);

    int marklin = Create(1, MarklinCommandTask);
    (void)marklin;

    Uart::Printf(uart, COM2, "Initializing Track..." ENDL);
    init_track(uart, marklin);

    // Clear any bytes in the COM1 FIFO so they aren't mistakenly treated as a
    // sensor query response.
    Uart::Drain(uart, COM1);
    Create(1, SensorReporterTask);
    Create(0, SensorPollerTask);

    int cmd_task_tid = Create(0, CmdTask);
    {
        CmdTaskCfg cfg = {.term_size = term_size};
        Send(cmd_task_tid, (char*)&cfg, sizeof(cfg), nullptr, 0);
    }
}
