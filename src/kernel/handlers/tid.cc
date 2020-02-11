#include "kernel/kernel.h"

namespace kernel::handlers {
int MyTid() { return current_task == UINT32_MAX ? -1 : (int)current_task; }

int MyParentTid() {
    if (!tasks[current_task].has_value()) return -1;
    return tasks[current_task].value().parent_tid.value_or(-1);
}
}  // namespace kernel::handlers
