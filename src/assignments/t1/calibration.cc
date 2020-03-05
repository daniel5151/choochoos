#include "calibration.h"

#include "user/debug.h"

namespace Calibration {
int expected_velocity(uint8_t train, uint8_t speed) {
    if (speed == 0) return 0;
    // TODO use calibration data
    (void)train;
    switch (speed) {
        case 8:
            return 200;
        case 14:
            return 600;
        default:
            panic("no expected velocity for train=%u speed=%u", train, speed);
    }
}

int stopping_distance(uint8_t train, uint8_t speed) {
    // TODO use our calibration data
    (void)train;
    (void)speed;
    return 250;  // 25 cm
}

int stopping_time(uint8_t train, uint8_t speed) {
    // TODO use our calibration data
    (void)train;
    (void)speed;
    return 300;  // 3 seconds
}

}  // namespace Calibration
