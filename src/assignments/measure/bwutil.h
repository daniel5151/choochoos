#include <cstddef>
#include <cstdint>

// busy waits for the specified number of ms using TIMER3
//
// PREREQ: TIMER3 must be set up to run at 508khz, and decrement from UINT_MAX
// This is the default in our kernel
void bwsleep(size_t ms);
