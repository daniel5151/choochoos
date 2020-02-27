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

void speed_test(uint8_t tr) {
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

    bwprintf(COM2,
             "// Hit [ENTER] once train %hhu is"
             " on the outer ring." ENDL,
             tr);
    bwgetline(nullptr, 1);

    bwprintf(COM2, "// Starting speed test..." ENDL);
    bwprintf(COM2, R"#({ "train": %hhu, "events": [)#" ENDL, tr);

    MarklinAction act;
    memset(&act, 0, sizeof(act));

    // turn the train off (for posterity)
    act.tag = MarklinAction::Train;
    act.train.no = tr;
    act.train.state._.speed = 0;
    act.bwexec();
    bwsleep(5000);

    for (uint8_t speed = 15; speed >= 1; speed--) {
        bool calibration = speed == 15;
        if (speed == 15) {
            speed = 14;
        }

        // start the train
        if (!calibration) {
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
            while (!bwgetnextsensor(s))
                ;
            if (!calibration) {
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

        bwsleep(2500); // let the train get up to speed 14, for consistency
        bwgetnextsensor(s); // clean any dummy sensors that may have triggered

        while (!bwgetnextsensor(s) ||
               !(s.group == 'A' && (s.idx == 3 || s.idx == 4)))
            ;

        bwprintf(COM2, "// Stopping train" ENDL);
        act.tag = MarklinAction::Train;
        act.train.no = tr;
        act.train.state._.speed = 0;
        act.bwexec();

        bwsleep(5000);  // ensure train has stopped
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

void FirstUserTask() {
    uint8_t tr;

    setup_com1();

    bwprintf(COM2, "// Enter train to test: ");
    char buf[16];
    bwgetline(buf, 16);
    sscanf(buf, "%hhu", &tr);

    speed_test(tr);
}
