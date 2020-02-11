#pragma once

#include <cstdint>

namespace kernel {

/// Helper POD struct which can should be casted from a void* that points to
/// a user's stack.
struct UserStack {
    uint32_t spsr;
    void* start_addr;
    uint32_t regs[13];
    void* lr;
    // C++ doesn't support flexible array members, so instead, we use an
    // array of size 1, and just do "OOB" memory access lol
    uint32_t additional_params[1];
};

}  // namespace kernel
