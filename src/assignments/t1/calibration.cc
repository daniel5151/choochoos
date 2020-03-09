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
    int idx = calibration_index_of_train((int)train);
    if (idx < 0) panic("no index for train %u", train);
    switch (speed) {
        case 0:
            return 0;
        // these are the speeds we've actually measured
        case 8:
        case 14: {
            const speed_level_t& speed_data =
                calibration.data.trains[idx].speeds[speed];
            assert(speed_data.measured_velocity);
            return speed_data.expected_velocity_mmps;
        }
        default: {
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
    }

    const speed_level_t& speed_data =
        calibration.data.trains[idx].speeds[speed];
    if (speed_data.measured_velocity) {
        return speed_data.expected_velocity_mmps;
    }
    panic("no expected velocity for train=%u speed=%u", train, speed);
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

int acceleration_time(uint8_t train, uint8_t target_speed) {
    // TODO measure this?
    (void)train;
    (void)target_speed;
    return 300;  // 3 seconds
}

}  // namespace Calibration
