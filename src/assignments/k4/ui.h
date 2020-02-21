#pragma once

#include <cstdint>

#include "common/ts7200.h"
#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/tasks/uartserver.h"

#include "trainctl.h"

namespace Ui {
void render_initial_screen(int uart);
void render_train_speed(int uart, uint8_t train, int speed);
void render_switch_direction(int uart, uint8_t sw, SwitchDir dir);
}  // namespace Ui
