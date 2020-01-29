#pragma once

#include <cstddef>
#include <cstdint>

enum class QueueErr : uint8_t { OK, FULL, EMPTY };

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

    QueueErr pop_front(T& dest) {
        if (len == 0) return QueueErr::EMPTY;

        dest = buf[start];
        start = (start + 1) % N;
        len--;
        return QueueErr::OK;
    }

    QueueErr peek_front(T& dest) const {
        if (len == 0) return QueueErr::EMPTY;

        dest = buf[start];
        return QueueErr::OK;
    }

    void clear() {
        start = 0;
        len = 0;
    }
};
