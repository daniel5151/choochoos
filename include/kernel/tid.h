#pragma once

#include <climits>
#include <cstddef>

namespace kernel {
class Tid final {
    size_t id;

   public:
    operator size_t() const { return this->id; }

    Tid(size_t id) : id{id} {}
    int raw_tid() const { return this->id; }
};
}  // namespace kernel
