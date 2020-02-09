#pragma once

#include <cstddef>
#include <optional>

template <class T, size_t N>
class OptArray {
    std::optional<T> arr[N];
    size_t size;

   public:
    OptArray() : arr{std::nullopt}, size{0} {}

    size_t num_present() const { return size; }

    void put(T t, size_t index) {
        if (index >= N) return;
        if (!arr[index].has_value()) size++;
        arr[index] = t;
    }

    bool has(size_t index) const {
        if (index >= N) return false;
        return arr[index].has_value();
    }

    std::optional<const T> get(size_t index) const {
        if (index >= N) return std::nullopt;
        if (arr[index].has_value()) return arr[index].value();
        return std::nullopt;
    }

    const std::optional<T>& operator[](size_t index) { return arr[index]; }
    const std::optional<const T>& operator[](size_t index) const {
        return arr[index];
    }

    std::optional<T> take(size_t index) {
        if (index >= N) return std::nullopt;
        if (arr[index].has_value()) {
            size--;
            T ret = arr[index].value();
            arr[index] = std::nullopt;
            return ret;
        }

        return std::nullopt;
    }
};
