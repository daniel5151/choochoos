#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

enum class QueueErr : uint8_t { OK, FULL };

template <class T, unsigned int N>
class Queue {
    size_t start;
    size_t len;
    T buf[N];

   public:
    Queue() : start(0), len(0), buf() {}
    size_t size() const { return len; }
    size_t available() const { return N - len; }
    bool is_empty() const { return len == 0; }

    QueueErr push_back(T t) {
        if (len >= N) return QueueErr::FULL;

        buf[(start + len) % N] = t;
        len++;
        return QueueErr::OK;
    }

    std::optional<T> pop_front() {
        if (len == 0) return std::nullopt;

        T ret = buf[start];
        start = (start + 1) % N;
        len--;
        return ret;
    }

    const T* peek_front() const {
        if (len == 0) return nullptr;
        return &buf[start];
    }

    const T* peek_index(size_t i) const {
        if (i >= len) return nullptr;
        return &buf[(start + i) % N];
    }

    void clear() {
        start = 0;
        len = 0;
    }
};
