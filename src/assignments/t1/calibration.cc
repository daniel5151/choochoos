#include "calibration.h"
#include "raw_calibration/calibration.h"

#include "user/debug.h"

/* private */ class StaticCalibrationData {
   public:
    calibration_data_t data;
    StaticCalibrationData() { fill_calibration_data(&this->data); }
};

namespace Calibration {
static const StaticCalibrationData calibration;

int expected_velocity(uint8_t train, uint8_t speed) {
    int idx = calibration_index_of_train(train);
    assert(idx >= 0);
    if (speed == 0) return 0;

    const speed_level_t& speed_data =
        calibration.data.trains[idx].speeds[speed];
    if (speed_data.measured_velocity) {
        return speed_data.expected_velocity_mmps;
    }
    panic("no expected velocity for train=%u speed=%u", train, speed);
}

int stopping_distance(uint8_t train, uint8_t speed) {
    int idx = calibration_index_of_train(train);
    assert(idx >= 0);
    if (speed == 0) return 0;

    const speed_level_t& speed_data =
        calibration.data.trains[idx].speeds[speed];
    if (speed_data.measured_stop_distance) {
        return speed_data.expected_stopping_distance_mm;
    }
    panic("no expected stopping distance for train=%u speed=%u", train, speed);
}

int stopping_time(uint8_t train, uint8_t speed) {
    // TODO use our calibration data
    (void)train;
    (void)speed;
    return 300;  // 3 seconds
}

}  // namespace Calibration
