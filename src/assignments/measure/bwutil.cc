#include "bwutil.h"

#include <cstddef>
#include <cstdint>

#include "common/ts7200.h"

const volatile uint32_t* TIMER3_VAL =
    (volatile uint32_t*)(TIMER3_BASE + VAL_OFFSET);

void bwsleep(size_t ms) {
    uint32_t idle_timer = *TIMER3_VAL;
    while (idle_timer - *TIMER3_VAL < (ms * 508));
}
