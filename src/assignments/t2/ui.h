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

/// Encapsulates all UI rendering logic
class Ui {
private:
    int uart;
    term_size_t term_size;

public:
    /// Creates a new Ui instance.
    ///
    /// Panics if the UART server hasn't been instantiated.
    Ui();

    /// Paints all static elements onto the screen
    void render_initial_screen() const;

    /// Render the train descriptor onto the screen
    void render_train_descriptor(const train_descriptor_t& td) const;

    /// Paint the command prompt, and wait for user to enter a command
    void prompt_user(char* buf, size_t len) const;
};
