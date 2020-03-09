#pragma once
// generated code, do not edit
// generated by 'python calibration/process.py'
// clang-format off

#include "stdbool.h"
#include "string.h" // memset

#ifdef __cplusplus
extern "C" {
#endif

#define CALIBRATION_NUM_TRAINS 6

struct speed_level_t {
  bool measured_velocity;
  int expected_velocity_mmps;
  bool measured_stop_distance;
  int expected_stopping_distance_mm;
  int max_stopping_distance_mm;
};

struct train_data_t {
  int train;
  struct speed_level_t speeds[15];
};

struct calibration_data_t {
  struct train_data_t trains[CALIBRATION_NUM_TRAINS];
};

int calibration_index_of_train(int train);
void fill_calibration_data(struct calibration_data_t* c);

#ifdef __cplusplus
}
#endif

