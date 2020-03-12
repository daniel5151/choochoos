#include "marklin.h"

#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

namespace Marklin {

void Controller::send_go() const { Uart::Putc(uart, COM1, 0x60); }
void Controller::send_stop() const { Uart::Putc(uart, COM1, 0x61); }

void Controller::update_train(TrainState tr) const {
    char msg[] = {(char)tr.raw, (char)tr.no, '\0'};
    Uart::Putstr(uart, COM1, msg);
}

void Controller::update_branch(uint8_t id, BranchDir dir) const {
    char d = dir == BranchDir::Curved ? 0x22 : 0x21;
    char msg[] = {d, (char)id, 0x20, '\0'};
    Uart::Putstr(uart, COM1, msg);
}

void Controller::update_branches(const BranchState* branches, size_t n) const {
    assert(n < 1024);  // don't blow the stack
    char msg[2 * n + 1];
    for (size_t i = 0; i < n; i++) {
        const BranchState& b = branches[i];
        msg[2 * i] = b.dir == BranchDir::Curved ? 0x22 : 0x21;
        msg[2 * i + 1] = (char)b.no;
    }
    msg[2 * n] = '\0';
    Uart::Putstr(uart, COM1, msg);
}

void Controller::query_sensors(char data[2 * NUM_SENSOR_GROUPS]) const {
    Uart::Drain(uart, COM1);
    Uart::Putc(uart, COM1, (char)(128 + NUM_SENSOR_GROUPS));
    // TODO?: make this resilient against dropped bytes
    Uart::Getn(uart, COM1, NUM_SENSOR_GROUPS * 2, data);
}

void Controller::flush() const { Uart::Flush(uart, COM1); }
}  // namespace Marklin
