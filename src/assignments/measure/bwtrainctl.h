#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint8_t VALID_SWITCHES[] = {1,  2,  3,   4,   5,   6,  7,  8,
                                      9,  10, 11,  12,  13,  14, 15, 16,
                                      17, 18, 153, 154, 155, 156};

constexpr uint8_t VALID_TRAINS[] = {1, 24, 58, 74, 78, 79};

constexpr size_t NUM_SENSOR_GROUPS = 5;

enum class SwitchDir : size_t { Straight, Curved };

union TrainState {
    uint8_t raw;
    struct {
        unsigned speed : 4;
        bool light : 1;
    } _;
};

struct MarklinAction {
    enum { Go, Stop, Train, Switch, QuerySensors } tag;
    union {
        struct {
        } go;
        struct {
        } stop;
        struct {
            size_t no;
            TrainState state;
        } train;
        struct {
            size_t no;
            SwitchDir dir;
        } sw;
        struct {
        } query_sensors;
    };

};

void do_marklin_action(const MarklinAction& act);
