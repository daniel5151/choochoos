#include "marklin.h"

#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

namespace Marklin {

void Controller::send_go() { Uart::Putc(uart, COM1, 0x60); }
void Controller::send_stop() { Uart::Putc(uart, COM1, 0x61); }

void Controller::update_train(TrainState tr) const {
    Uart::Putc(uart, COM1, (char)tr.raw);
    Uart::Putc(uart, COM1, (char)tr.no);
}

void Controller::update_branches(const BranchState* branches, size_t n) const {
    for (size_t i = 0; i < n; i++) {
        const BranchState& b = branches[i];
        Uart::Putc(uart, COM1, b.dir == BranchState::Dir::Curved ? 0x22 : 0x21);
        Uart::Putc(uart, COM1, (char)b.no);
    }
    Uart::Putc(uart, COM1, 0x20);
}

void Controller::query_sensors(char data[2 * NUM_SENSOR_GROUPS]) const {
    Uart::Putc(uart, COM1, (char)(128 + NUM_SENSOR_GROUPS));
    // TODO?: make this resilient against dropped bytes
    Uart::Getn(uart, COM1, NUM_SENSOR_GROUPS * 2, data);
}

void Controller::flush() const {
    Uart::Flush(uart, COM1);
}
}
