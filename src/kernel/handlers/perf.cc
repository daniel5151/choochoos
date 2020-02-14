#include "kernel/kernel.h"

namespace kernel::handlers {

void Perf(user::perf_t* perf) {
    if (perf == nullptr) {
        return;
    }

    perf->idle_time_pct = kernel::idle_time_pct;
}

}  // namespace kernel::handlers
