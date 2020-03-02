#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "common/bwio.h"
#include "common/ts7200.h"
#include "user/debug.h"
#include "user/syscalls.h"

#include "bwtrainctl.h"
#include "bwutil.h"

static const volatile uint32_t* TIMER3_VAL =
    (volatile uint32_t*)(TIMER3_BASE + VAL_OFFSET);

uint32_t currtime() { return UINT32_MAX - *TIMER3_VAL; }

void init_track() {
    MarklinAction act;
    memset(&act, 0, sizeof(act));

    // stop all the trains
    act.tag = MarklinAction::Train;
    act.train.state._.speed = 0;
    act.train.state._.light = 0;
    for (uint8_t no : VALID_TRAINS) {
        bwprintf(COM2, "// Stopping train %hhu" ENDL, no);
        act.train.no = no;
        act.bwexec();
    }

    // set all the switches to curved
    act.tag = MarklinAction::Switch;
    act.sw.dir = SwitchDir::Curved;
    for (uint8_t no : VALID_SWITCHES) {
        bwprintf(COM2, "// Toggling switch %hhu" ENDL, no);
        act.sw.no = no;
        act.bwexec();
    }
}

void make_outer_loop() {
    MarklinAction act;
    memset(&act, 0, sizeof(act));

    act.tag = MarklinAction::Switch;
    act.sw.dir = SwitchDir::Straight;
    for (size_t no : {6, 7, 8, 9, 14, 15}) {
        act.sw.no = no;
        act.bwexec();
    }
}

struct sensor_t {
    char group;
    uint8_t idx;

    bool operator==(const sensor_t& other) const {
        return this->group == other.group && this->idx == other.idx;
    }

    bool operator!=(const sensor_t& other) const { return !(*this == other); }
};

bool bwgetnextsensor(sensor_t& s) {
    char bytes[NUM_SENSOR_GROUPS * 2];

    bwputc(COM1, (char)(128 + NUM_SENSOR_GROUPS));
    for (char& c : bytes) {
        c = (char)bwgetc(COM1);
    }

    for (size_t bi = 0; bi < NUM_SENSOR_GROUPS * 2; bi++) {
        char byte = bytes[bi];
        for (size_t i = 1; i <= 8; i++) {
            if ((byte >> (8 - i)) & 0x01) {
                s.group = (char)((int)'A' + (bi / 2));
                s.idx = (uint8_t)(i + (8 * (bi % 2)));
                return true;
            }
        }
    }

    return false;
}

void set_up_track(uint8_t tr) {
    bwprintf(COM2, "// Reset track (y/n): ");
    char reset[2];
    bwgetline(reset, 2);
    if (reset[0] == 'y') {
        bwprintf(COM2, "// Setting up track for speed test..." ENDL);
        // reset the track...
        init_track();
        // ...with a loop in the center
        make_outer_loop();
    }

    bwprintf(COM2, "// Hit [ENTER] once train %hhu is on the outer ring." ENDL,
             tr);
    bwgetline(nullptr, 1);
}

void speed_test(uint8_t tr, uint8_t start_speed) {
    set_up_track(tr);

    bwprintf(COM2, "// Starting speed test..." ENDL);
    bwprintf(COM2, R"#({ "train": %hhu, "events": [)#" ENDL, tr);

    MarklinAction act;
    memset(&act, 0, sizeof(act));

    // turn the train off (for posterity)
    bwprintf(COM2, "// Stopping train..." ENDL);
    act.tag = MarklinAction::Train;
    act.train.no = tr;
    act.train.state._.speed = 0;
    act.bwexec();
    bwsleep(5000);

    bool calibration_round = true;
    bwprintf(COM2, "// Doing calibration round..." ENDL);
    for (uint8_t speed = start_speed; speed >= 1; speed--) {
        // start the train
        if (!calibration_round) {
            bwprintf(COM2, R"#({"event":"speed","val":%hhu,"time":%lu},)#" ENDL,
                     speed, currtime());
        }

        act.tag = MarklinAction::Train;
        act.train.no = tr;
        act.train.state._.speed = speed & 0x0f;
        act.bwexec();

        sensor_t s;

        uint32_t start_time = *TIMER3_VAL;
        while (start_time - *TIMER3_VAL < 508 * 13000) {  // 13 seconds
            // ensures we get at least once sensor
            while (!bwgetnextsensor(s))
                ;
            if (!calibration_round) {
                bwprintf(
                    COM2,
                    R"#({"event":"sensor","speed":%hhu,"sensor":"%c%hhu","time":%lu},)#" ENDL,
                    speed, s.group, s.idx, currtime());
            }
        }

        // bring the speed back up to 14, but stop once the train hits a
        // known-good stopping point (i.e: one that's not on a curve)
        bwprintf(COM2, "// Done collecting! Sending the train to A3..." ENDL);
        act.tag = MarklinAction::Train;
        act.train.no = tr;
        act.train.state._.speed = 14;
        act.bwexec();

        bwsleep(2500);  // let the train get up to speed 14, for consistency
        // clean any dummy sensors that may have triggered
        while (!bwgetnextsensor(s))
            ;

        while (!bwgetnextsensor(s) ||
               !(s.group == 'A' && (s.idx == 3 || s.idx == 4)))
            ;

        bwprintf(COM2, "// Stopping train" ENDL);
        act.tag = MarklinAction::Train;
        act.train.no = tr;
        act.train.state._.speed = 0;
        act.bwexec();

        bwsleep(5000);  // ensure train has stopped

        if (calibration_round) {
            speed = (uint8_t)(start_speed + 1);  // re-do test
            calibration_round = false;
        }
    }

    bwprintf(COM2, "]}" ENDL);
}

void setup_com1() {
    volatile int* mid = (volatile int*)(UART1_BASE + UART_LCRM_OFFSET);
    volatile int* low = (volatile int*)(UART1_BASE + UART_LCRL_OFFSET);
    volatile int* high = (volatile int*)(UART1_BASE + UART_LCRH_OFFSET);

    *mid = 0x0;
    *low = 0xbf;

    int buf = *high;
    buf = (buf | STP2_MASK) & ~FEN_MASK;
    *high = buf;
}

void stopping_distance_test(uint8_t tr, uint8_t speed) {
    set_up_track(tr);

    char line[10] = {0};
    MarklinAction act;
    memset(&act, 0, sizeof(act));

    act.tag = MarklinAction::Train;
    act.train.no = tr;

    bwprintf(COM2, R"#({ "train": %hhu, "events": [)#" ENDL, tr);
    while (true) {
        sensor_t s;
        memset(&s, 0, sizeof(s));

        act.train.state._.speed = speed & 0xf;
        act.bwexec();
        bwprintf(COM2, R"#({"event":"speed","val":%hhu,"time":%lu},)#" ENDL,
                 speed, currtime());

        while (!(s.group == 'E' && (s.idx == 11 || s.idx == 12))) {
            while (!bwgetnextsensor(s)) {
            }
            bwprintf(
                COM2,
                R"#({"event":"sensor","speed":%hhu,"sensor":"%c%hhu","time":%lu},)#" ENDL,
                speed, s.group, s.idx, currtime());
        }

        act.train.state._.speed = 0;
        act.bwexec();
        bwprintf(COM2, R"#({"event":"stop","time":%lu},)#" ENDL, currtime());
        bwprintf(COM2, "// press any key once stopped");
        bwgetc(COM2);
        uint32_t stopped_time = currtime();

        bwprintf(COM2, ENDL "// distance from E11/E12 (cm): ");
        bwgetline(line, sizeof(line));
        int distance;
        sscanf(line, "%d", &distance);

        bwprintf(COM2,
                 R"#({"event":"stopped","distance_cm":%d,"time":%lu},)#" ENDL,
                 distance, stopped_time);

        bwputstr(COM2, "// another one? (y/n): ");
        bwgetline(line, sizeof(line));
        if (strcmp(line, "y") != 0) break;
        bwprintf(COM2, "// speed (blank for %hhu): ", speed);
        bwgetline(line, sizeof(line));
        int spd = (int)speed;
        sscanf(line, "%d", &spd);
        speed = (uint8_t)spd;

        // clear any read sensors
        bwgetnextsensor(s);
    }
    bwprintf(COM2, "]}" ENDL);
}

void FirstUserTask() {
    int tr;
    int start_speed;
    char buf[16] = {0};

    setup_com1();

    bwprintf(COM2, "// Enter train to test: ");
    bwgetline(buf, 16);
    sscanf(buf, "%d", &tr);
    //    bwprintf(COM2, "you entered %d (ret=%d)" ENDL, tr, ret);

    bwprintf(COM2, "// Enter start speed for data collection: ");
    bwgetline(buf, 16);
    sscanf(buf, "%d", &start_speed);
    //   bwprintf(COM2, "you entered %d (ret=%d)" ENDL, start_speed, ret);

    // speed_test(tr, start_speed);
    stopping_distance_test((uint8_t)tr, (uint8_t)start_speed);
}
