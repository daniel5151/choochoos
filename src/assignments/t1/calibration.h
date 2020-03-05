#pragma once

#include <cstdint>

namespace Calibration {
int /* mm/s */ expected_velocity(uint8_t train, uint8_t speed);
int /* mm */ stopping_distance(uint8_t train, uint8_t speed);
int /* ticks */ stopping_time(uint8_t train, uint8_t speed);
}  // namespace Calibration
