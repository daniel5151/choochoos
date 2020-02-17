#pragma once

enum class SwitchDir : size_t {
    Straight,
    Curved
};

union TrainCmd {
    char raw;
    struct {
        unsigned speed : 4;
        unsigned light : 1;
    } _;
};

void update_train_speed(size_t no, size_t speed);
void update_train_light(size_t no, bool active);
void update_switch_pos(size_t sw_no, SwitchDir dir);

void send_stop_cmd();
void send_go_cmd();
void send_sw_seq_end();
void set_sensor_mode_to_reset();

void req_sensor_data();
