#pragma once

#include <cstddef>
#include <cstdint>

const uint8_t VALID_SWITCHES[] = {1,  2,  3,   4,   5,   6,  7,  8,
                                  9,  10, 11,  12,  13,  14, 15, 16,
                                  17, 18, 153, 154, 155, 156};
constexpr size_t NUM_VALID_SWITCHES =
    sizeof(VALID_SWITCHES) / sizeof(VALID_SWITCHES[0]);

const uint8_t VALID_TRAINS[] = {1, 24, 58, 74, 78, 79};
constexpr size_t NUM_VALID_TRAINS =
    sizeof(VALID_TRAINS) / sizeof(VALID_TRAINS[0]);

enum class SwitchDir : size_t { Straight, Curved };

union TrainState {
    uint8_t raw;
    struct {
        unsigned speed : 4;
        bool light : 1;
    } _;
};
