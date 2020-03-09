#include "calibration.h"
#include "raw_calibration/calibration.h"

#include "user/debug.h"

#include <cstdlib>  // abs

/* private */ class StaticCalibrationData {
   public:
    calibration_data_t data;
    StaticCalibrationData() { fill_calibration_data(&this->data); }
};

namespace Calibration {
static const StaticCalibrationData calibration;

int expected_velocity(uint8_t train, uint8_t speed) {
    int idx = calibration_index_of_train((int)train);
    if (idx < 0) panic("no index for train %u", train);

    if (speed == 0) return 0;

    const speed_level_t& speed_data =
        calibration.data.trains[idx].speeds[speed];
    if (speed_data.measured_velocity) {
        return speed_data.expected_velocity_mmps;
    }

    auto& speed_8 = calibration.data.trains[idx].speeds[8];
    auto& speed_14 = calibration.data.trains[idx].speeds[14];
    assert(speed_8.measured_velocity);
    assert(speed_14.measured_velocity);
    int speed_8_vel = speed_8.expected_velocity_mmps;
    int speed_14_vel = speed_14.expected_velocity_mmps;

    if (speed > 8) {
        // 8 < speed < 14: interpolate between the two measured speeds
        return speed_8_vel +
               (speed - 8) * (speed_14_vel - speed_8_vel) / (14 - 8);
    }

    // 0 < speed < 8: interpolate between 0 and speed_8_vel
    return speed * speed_8_vel / 8;
}

int stopping_distance(uint8_t train, uint8_t speed) {
    int idx = calibration_index_of_train((int)train);
    if (idx < 0) panic("no index for train %u", train);
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

int acceleration_time(uint8_t train,
                      int current_velocity,
                      uint8_t target_speed) {
    int v_expected = expected_velocity(train, target_speed);
    int max_change_in_velocity = expected_velocity(train, 14);
    int change_in_velocity = std::abs(v_expected - current_velocity);
    int time_to_accelerate_to_full_speed = 300;  // 3 seconds, rough estimate
    return time_to_accelerate_to_full_speed * change_in_velocity /
           max_change_in_velocity;
}

}  // namespace Calibration
