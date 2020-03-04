#include "ui.h"

#include <cstring>  // memset

#include "common/bwio.h"
#include "common/ts7200.h"
#include "user/tasks/uartserver.h"

namespace Ui {

static constexpr int MAX_TRAINS = 2;

void render_initial_screen(int uart, const TermSize& term_size) {
    char sep[term_size.width + 1];
    memset(sep, '-', term_size.width);
    sep[term_size.width] = '\0';

    int scroll_start = MAX_TRAINS + 5;
    int scroll_end = term_size.height;
    int sep_row = scroll_start - 1;

    Uart::Printf(uart, COM2,
                 VT_CLEAR VT_SET_SCROLL_FMT VT_ROWCOL_FMT
                 "%s" ENDL VT_ROWCOL_FMT "%s" ENDL VT_ROWCOL_FMT,
                 scroll_start, scroll_end, sep_row, 1, sep, sep_row - 2, 1, sep,
                 term_size.height, 1);
}

void render_train_descriptor(int uart, const train_descriptor_t& td) {
    char line[256] = {0};

    int row = 3;  // TODO when rendering multiple trains, make this variable
    int col = 1;

    size_t n = snprintf(
        line, sizeof(line),
        VT_SAVE VT_ROWCOL_FMT VT_CLEARLN
        "[%5d] train=%2hhu spd=%2d vel=%4dmm/s pos=%c%02hhu ofst=%3dmm",
        row, col, td.pos_observed_at, td.id, td.speed, td.velocity,
        td.pos.sensor.group, td.pos.sensor.idx, td.pos.offset_mm);
    if (td.has_next_sensor) {
        auto sensor = td.next_sensor;
        int time = td.next_sensor_time;
        n += snprintf(line + n, sizeof(line) - n, " next=(%c%02hhu at t=%5d)",
                      sensor.group, sensor.idx, time);
    }
    if (td.has_error) {
        char sign = td.time_error < 0 ? '-' : '+';
        int error_sec = std::abs(td.time_error) / TICKS_PER_SEC;
        int error_dec = std::abs(td.time_error) % TICKS_PER_SEC;

        n += snprintf(line + n, sizeof(line) - n, " error=%c%d.%02ds/%c%dmm",
                      sign, error_sec, error_dec, sign,
                      std::abs(td.distance_error));
    }
    n += snprintf(line + n, sizeof(line) - n, ENDL VT_RESTORE);

    Uart::Putstr(uart, COM2, line);
}


void prompt_user(int uart, char* buf, size_t len) {
    Uart::Drain(uart, COM2); // clear any input entered while prompt was busy
    Uart::Putstr(uart, COM2, VT_ROWCOL(5, 1) VT_CLEARLN "> ");
    Uart::Getline(uart, COM2, buf, len);
    Uart::Printf(uart, COM2, VT_ROWCOL(5, 1) VT_CLEARLN "Processing...");
}

}  // namespace Ui
