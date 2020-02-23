#include "ui.h"

#include <cstdint>

#include "common/ts7200.h"
#include "common/vt_escapes.h"
#include "user/debug.h"
#include "user/tasks/uartserver.h"

namespace Ui {

void render_initial_screen(int uart) {
    Uart::Printf(uart, COM2,
                 VT_SAVE VT_ROWCOL(5, 1) "Train |" ENDL
                                         "Speed |" ENDL VT_RESTORE);
    size_t i = 0;
    for (uint8_t train : VALID_TRAINS) {
        int row = 5;
        int col = 8 + i * 6;
        Uart::Printf(uart, COM2,
                     VT_SAVE VT_ROWCOL_FMT " %3u |" VT_DOWN(1)
                         VT_LEFT(1) "|" VT_RESTORE,
                     row, col, train);
        i++;
    }

    Uart::Printf(uart, COM2,
                 VT_SAVE VT_ROWCOL(8, 1) "Switches" ENDL "|" ENDL "|" ENDL
                                         "|" VT_RESTORE);

    i = 0;
    for (uint8_t sw : VALID_SWITCHES) {
        int row = 9 + i / 8;
        int col = 2 + 6 * (i % 8);
        Uart::Printf(uart, COM2, VT_SAVE VT_ROWCOL_FMT "%3u %c|" VT_RESTORE,
                     row, col, sw, '?');
        i++;
    }
}

void render_train_speed(int uart, uint8_t train, int speed) {
    size_t i = 0;
    for (; i < NUM_VALID_TRAINS; i++) {
        if (VALID_TRAINS[i] == train) break;
    }
    if (i == NUM_VALID_TRAINS) return;

    int row = 6;
    int col = 8 + i * 6 + 1;
    const char* color = speed == 0 ? VT_RED : VT_GREEN;
    Uart::Printf(uart, COM2, VT_SAVE VT_ROWCOL_FMT "%s%3d" VT_NOFMT VT_RESTORE,
                 row, col, color, speed);
}

void render_switch_direction(int uart, uint8_t sw, SwitchDir dir) {
    size_t i = 0;
    for (; i < NUM_VALID_SWITCHES; i++) {
        if (VALID_SWITCHES[i] == sw) break;
    }
    if (i == NUM_VALID_SWITCHES) return;

    int row = 9 + i / 8;
    int col = 2 + 6 * (i % 8) + 4;
    const char* color = dir == SwitchDir::Straight ? VT_BLUE : VT_YELLOW;
    char c = dir == SwitchDir::Straight ? 'S' : 'C';

    Uart::Printf(uart, COM2, VT_SAVE VT_ROWCOL_FMT "%s%c" VT_NOFMT VT_RESTORE,
                 row, col, color, c);
}
}  // namespace Ui
