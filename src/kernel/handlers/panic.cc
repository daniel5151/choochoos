#include "kernel/kernel.h"
#include "kernel/helpers.h"

namespace kernel::handlers {

void Panic() {
    kpanic("user task (tid=%d) panicked!\r\n", ::kernel::handlers::MyTid());
}

}  // namespace kernel::handlers
