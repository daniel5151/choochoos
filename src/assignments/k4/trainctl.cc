#include "trainctl.h"

#include "user/debug.h"
#include "user/syscalls.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

void MarklinCommandTask() {
    int uart = WhoIs(Uart::SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);

    assert(uart >= 0);
    assert(clock >= 0);

    int nsres = RegisterAs("MarklinCommandTask");
    assert(nsres >= 0);

    MarklinAction act;
    int tid;
    for (;;) {
        Receive(&tid, (char*)&act, sizeof(act));
        switch (act.tag) {
            case MarklinAction::Go:
                Uart::Putc(uart, COM1, 0x60);
                break;
            case MarklinAction::Stop:
                Uart::Putc(uart, COM1, 0x61);
                break;
            case MarklinAction::Train:
                Uart::Putc(uart, COM1, (char)act.train.state.raw);
                Uart::Putc(uart, COM1, (char)act.train.no);
                break;
            case MarklinAction::Switch:
                Uart::Putc(uart, COM1,
                           act.sw.dir == SwitchDir::Straight ? 0x21 : 0x22);
                Uart::Putc(uart, COM1, (char)act.sw.no);
                break;
            case MarklinAction::QuerySensors:
                // Drain will clear any bytes that have been received but not
                // yet reported by the SensorReporterTask. This means that if we
                // drop any bytes while reading from COM1, the incomplete
                // response won't stall all future responses from being
                // received.
                Uart::Drain(uart, COM1);
                Uart::Putc(uart, COM1, (char)(128 + NUM_SENSOR_GROUPS));
                break;
            default:
                panic("MarklinCommandTask received an invalid action");
        }
        Reply(tid, nullptr, 0);

        // Ensure that last command completed before receiving another.
        Uart::Flush(uart, COM1);
    }
}
