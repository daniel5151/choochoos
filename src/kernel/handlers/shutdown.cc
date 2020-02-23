#include "kernel/kernel.h"
#include "kernel/helpers.h"

namespace kernel::handlers {

void Shutdown() {
    kexit(0);
}

}  // namespace kernel::handlers
