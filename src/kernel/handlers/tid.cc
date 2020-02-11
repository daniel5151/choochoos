#include "kernel/kernel.h"

namespace kernel {
int Kernel::MyTid() { return this->current_task; }

int Kernel::MyParentTid() {
    if (!this->tasks[current_task].has_value()) return -1;
    return this->tasks[current_task].value().parent_tid.value_or(-1);
}
}  // namespace kernel
