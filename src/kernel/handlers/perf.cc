#include "kernel/kernel.h"

#include "common/ts7200.h"

namespace kernel::handlers {

void Perf(user::perf_t* perf) {
    kernel::perf::last_perf_call_time =
        *(volatile uint32_t*)(TIMER3_BASE + VAL_OFFSET);

    if (perf == nullptr) {
        return;
    }

    perf->idle_time_pct = kernel::perf::idle_time::pct;

    kernel::perf::idle_time::counter = 0;
    kernel::perf::idle_time::pct = 0;
}

}  // namespace kernel::handlers
