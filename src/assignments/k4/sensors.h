#pragma once

#include <cstddef>
namespace Sensors {
static const size_t NUM_SENSOR_GROUPS = 5;

struct Response {
    char bytes[NUM_SENSOR_GROUPS * 2];
};

// ReaderTask continuously sends well-formed Response structs to its parent.
void ReaderTask();
void SendQuery(int uart);
}  // namespace Sensors
