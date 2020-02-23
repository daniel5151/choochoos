#pragma once

#include "trainctl.h"

namespace Ui {
void render_initial_screen(int uart);
void render_train_speed(int uart, uint8_t train, int speed);
void render_switch_direction(int uart, uint8_t sw, SwitchDir dir);
}  // namespace Ui
