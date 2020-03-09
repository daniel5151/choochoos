#pragma once

#include "common/vt_escapes.h"
#include "track_oracle.h"

#define log_line(uart, fmt, ...)                                            \
    Uart::Printf(uart, COM2, VT_SAVE VT_ROWCOL(999, 1) fmt ENDL VT_RESTORE, \
                 ##__VA_ARGS__)

#define log_colored_line(uart, color, fmt, ...)                                \
    Uart::Printf(uart, COM2,                                                   \
                 VT_SAVE VT_ROWCOL(999, 1) color fmt VT_NOFMT ENDL VT_RESTORE, \
                 ##__VA_ARGS__)

#define log_success(uart, fmt, ...) \
    log_colored_line(uart, VT_CYAN, fmt, ##__VA_ARGS__)

#define log_warning(uart, fmt, ...) \
    log_colored_line(uart, VT_YELLOW, fmt, ##__VA_ARGS__)

#define log_prompt(uart, fmt, ...) \
    log_colored_line(uart, VT_GREEN, fmt, ##__VA_ARGS__)

#define log_error(uart, fmt, ...) \
    log_colored_line(uart, VT_RED, fmt, ##__VA_ARGS__)

namespace Ui {
void render_initial_screen(int uart, const TermSize& term_size);
void render_train_descriptor(int uart, const train_descriptor_t& td);
void prompt_user(int uart, char* buf, size_t len);

}  // namespace Ui
