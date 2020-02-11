#include "kernel/kernel.h"

namespace kernel::handlers {
void Yield() { kdebug("Called Yield"); }
}  // namespace kernel::handlers
