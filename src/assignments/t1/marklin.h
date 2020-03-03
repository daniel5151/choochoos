#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

/// Classes for interacting with the Marklin digital train controller.
namespace Marklin {

constexpr uint8_t VALID_SWITCHES[] = {1,  2,  3,   4,   5,   6,  7,  8,
                                      9,  10, 11,  12,  13,  14, 15, 16,
                                      17, 18, 153, 154, 155, 156};
constexpr uint8_t VALID_TRAINS[] = {1, 24, 58, 74, 78, 79};
constexpr size_t NUM_SENSOR_GROUPS = 5;

enum class Track { A, B };

enum class BranchDir { Straight, Curved };

/// Describes a particular track sensor
struct sensor_t {
    char group;
    uint8_t idx;
};

/// Wrapper around raw sensor data
struct SensorData {
    char raw[2 * NUM_SENSOR_GROUPS];

    std::optional<sensor_t> next_sensor() {
        for (size_t bi = 0; bi < NUM_SENSOR_GROUPS * 2; bi++) {
            char& byte = raw[bi];
            for (size_t i = 1; i <= 8; i++) {
                bool active = (byte >> (8 - i)) & 0x01;
                // disable the bit
                byte &= (char)(~(0x01 << (8 - i)));

                if (active) {
                    char group = (char)((int)'A' + (bi / 2));
                    uint8_t idx = (uint8_t)(i + (8 * (bi % 2)));

                    const sensor_t s = {.group = group, .idx = idx};
                    return s;
                }
            }
        }

        return std::nullopt;
    }
};

/// Describes a position on the track
struct track_pos_t {
    sensor_t sensor;
    int offset_mm;
};

/// Describes the state of a particular train (i.e: speed, lights, horn, etc...)
class TrainState {
    friend class Controller;

    uint8_t no;
    uint8_t raw;

   public:
    TrainState() = delete;
    TrainState(uint8_t id) : no{id} {}
    TrainState(uint8_t id, uint8_t speed, bool light = false) : no{id} {
        set_speed(speed);
        set_light(light);
    }

    uint8_t get_id() const { return no; }
    uint8_t get_speed() const { return (uint8_t)(raw & 0x0f); }
    bool get_light() const { return (bool)(raw & 0x10); }

    void set_id(uint8_t id) { no = id; }
    void set_speed(uint8_t speed) {
        raw = (uint8_t)((raw & ~0x0f) | (speed & 0x0f));
    }
    void set_light(bool active) {
        raw = (uint8_t)((raw & ~0x10) | (active << 4));
    }
};

/// Describes the state of a particular branch (straight / curved)
class BranchState {
   private:
    friend class Controller;

    uint8_t no;
    BranchDir dir;

   public:
    BranchState() = default;
    BranchState(uint8_t id, BranchDir branch_dir) : no{id}, dir{branch_dir} {}

    uint8_t get_branch() const { return no; }
    BranchDir get_dir() const { return dir; }

    void set_id(uint8_t id) { no = id; }
    void set_dir(BranchDir branch_dir) { dir = branch_dir; }
};

/// High-level abstraction for interacting with the Marklin train controller
/// via the UART controller.
class Controller {
   private:
    const int uart;

   public:
    /// Construct a new MarklinUART.
    Controller(int uart_tid) : uart{uart_tid} {}

    /// Sends the Go command.
    void send_go() const;
    /// Sends the Emergency Stop command.
    void send_stop() const;

    /// Sends the commands to update a particular train's state.
    void update_train(TrainState tr) const;
    /// Sends the commands to update a particular branch.
    void update_branch(uint8_t id, BranchDir dir) const;
    /// Sends the commands to update a set of branches.
    void update_branches(const BranchState* branches, size_t n) const;

    /// Sends a sensor query command, blocking until the train set responds with
    /// the sensor data.
    void query_sensors(char data[2 * NUM_SENSOR_GROUPS]) const;

    /// Block until the UART has finished sending any queued commands
    void flush() const;
};
}  // namespace Marklin
