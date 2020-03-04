#pragma once

#include "common/vt_escapes.h"
#include "track_oracle.h"

namespace Ui {
void render_initial_screen(int uart, const TermSize& term_size);
void render_train_descriptor(int uart, const train_descriptor_t& td);
}  // namespace Ui
