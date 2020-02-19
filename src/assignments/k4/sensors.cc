#include "sensors.h"

#include <cstring>

#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

namespace Sensors {

#define SENSOR_TIMEOUT 10  // 100ms

struct Message {
    enum uint8_t { Byte = 1, Timeout = 2 } tag;
    union {
        char byte;
        struct {
        } timeout;
    };
};

static void ReadSendLoop() {
    int uart = WhoIs(Uart::SERVER_ID);
    assert(uart >= 0);

    int myparent = MyParentTid();

    Message m{.tag = Message::Byte, .byte = 0};
    while (true) {
        int res = Uart::Getc(uart, COM1);
        if (res < 0)
            panic("Sensors::ReadSendLoop: could not read byte from COM1: %d",
                  res);
        m.byte = (char)res;

        Send(myparent, (char*)&m, sizeof(m), nullptr, 0);
    }
}

static void TimeoutTask() {
    int parent = MyParentTid();
    int clock = WhoIs(Clock::SERVER_ID);
    assert(clock >= 0);
    Clock::Delay(clock, SENSOR_TIMEOUT);
    Message m = {.tag = Message::Timeout, .timeout = {}};
    Send(parent, (char*)&m, sizeof(m), nullptr, 0);
}

void ReaderTask() {
    int sendloop = Create(1000, ReadSendLoop);
    assert(sendloop >= 0);
    int parent = MyParentTid();

    Message m;
    memset(&m, 0, sizeof(m));

    Response res;

    int tid = -1;
    while (true) {
        // TODO this timeout shouldn't start until we either
        // 1. receive the first byte
        // 2. send out the sensor query
        //
        // otherwise we're starting the timeouts on arbitrary 100ms time slots,
        // which might not coincide with a sensor query.
        //
        // we could instead implement a blocking QuerySensors() function that
        // sends the query, starts the timeout, and grabs the response. The
        // caller would be guaranteed that a response would come back in 100ms
        // (or 70ms, or whatever we set).
        int timeout_task = Create(1, TimeoutTask);
        assert(timeout_task >= 0);
        for (size_t i = 0; i < NUM_SENSOR_GROUPS * 2; i++) {
            // Drop any messages that aren't from the send loop or the most
            // recent timeout task.
            do {
                Receive(&tid, (char*)&m, sizeof(m));
                Reply(tid, nullptr, 0);
            } while (tid != sendloop && tid != timeout_task);
            switch (m.tag) {
                case Message::Byte:
                    res.bytes[i] = m.byte;
                    break;
                case Message::Timeout:
                    goto skip_loop;
                default:
                    panic("Sensors::ReaderTask: bad tag %d", m.tag);
            }
        }

        Send(parent, (char*)&res, sizeof(res), nullptr, 0);
    skip_loop:;
    }
}

void SendQuery(int uart) {
    int res = Uart::Putc(uart, COM1, 128 + NUM_SENSOR_GROUPS);
    assert(res == 1);
}
}  // namespace Sensors
