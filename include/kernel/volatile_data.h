#pragma once

#include <cstdint>

namespace kernel {

class VolatileData final {
    uint32_t data;

   public:
    VolatileData(uint32_t data) : data{data} {}
    uint32_t raw() const { return data; }
};

}  // namespace kernel
