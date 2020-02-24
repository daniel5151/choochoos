#pragma once

#include <cstdint>

enum class SwitchDir : size_t {
    Straight,
    Curved
};

union TrainState {
    uint8_t raw;
    struct {
        unsigned speed : 4;
        bool     light : 1;
    } _;
};
