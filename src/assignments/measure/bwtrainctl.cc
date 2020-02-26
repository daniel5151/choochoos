#include "bwtrainctl.h"

#include "common/bwio.h"
#include "user/debug.h"
#include "user/syscalls.h"

#include "bwutil.h"

void do_marklin_action(const MarklinAction& act) {
    switch (act.tag) {
        case MarklinAction::Go:
            bwputc(COM1, 0x60);
            break;
        case MarklinAction::Stop:
            bwputc(COM1, 0x61);
            break;
        case MarklinAction::Train:
            bwputc(COM1, (char)act.train.state.raw);
            bwputc(COM1, (char)act.train.no);
            break;
        case MarklinAction::Switch:
            bwputc(COM1, act.sw.dir == SwitchDir::Straight ? 0x21 : 0x22);
            bwputc(COM1, (char)act.sw.no);
            break;
        case MarklinAction::QuerySensors:
            bwputc(COM1, (char)(128 + NUM_SENSOR_GROUPS));
            // NOTE: do _not_ bwsleep if action is to read in sensor data!
            return;
        default:
            panic("MarklinAction::bwexec called with an invalid action");
    }

    bwsleep(250);
}
