#pragma once

#include "common/vt_escapes.h"
#include "track_oracle.h"

#define log_line(ui, fmt, ...)                                           \
    Uart::Printf(ui._uart(), COM2,                                       \
                 VT_SAVE VT_ROWCOL(999, 1) "[%5d] " fmt ENDL VT_RESTORE, \
                 Clock::Time(ui._clock()), ##__VA_ARGS__)

#define log_colored_line(ui, color, fmt, ...)           \
    Uart::Printf(ui._uart(), COM2,                      \
                 VT_SAVE VT_ROWCOL(999, 1) color        \
                 "[%5d] " fmt VT_NOFMT ENDL VT_RESTORE, \
                 Clock::Time(ui._clock()), ##__VA_ARGS__)

#define log_success(ui, fmt, ...) \
    log_colored_line(ui, VT_CYAN, fmt, ##__VA_ARGS__)

#define log_warning(ui, fmt, ...) \
    log_colored_line(ui, VT_YELLOW, fmt, ##__VA_ARGS__)

#define log_useraction(ui, fmt, ...) \
    log_colored_line(ui, VT_GREEN, fmt, ##__VA_ARGS__)

#define log_error(ui, fmt, ...) log_colored_line(ui, VT_RED, fmt, ##__VA_ARGS__)

/// Encapsulates all UI rendering logic
class Ui {
   private:
    int uart;
    int clock;
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

    /// be a good citizen, and reset the entire screen
    void shutdown() const;

    /// HACK: remove once we find a good way to do a variadic log method
    inline int _uart() const { return uart; };
    inline int _clock() const { return clock; };
};
