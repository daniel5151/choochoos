#pragma once

#include <cstddef>
#include <cstdint>

enum QueueErr { OK, FULL, EMPTY };

template <class T, unsigned int N>
class Queue {
    size_t start;
    size_t len;
    T buf[N];

   public:
    Queue() : start(0), len(0) {}
    size_t size() const { return len; }
    size_t available() const { return N - len; }
    bool is_empty() const { return len == 0; }

    QueueErr push_back(T t) {
        if (len >= N) return FULL;

        buf[(start + len) % N] = t;
        len++;
        return OK;
    }

    QueueErr pop_front(T& dest) {
        if (len == 0) return EMPTY;

        dest = buf[start];
        start = (start + 1) % N;
        len--;
        return OK;
    }

    QueueErr peek_front(T& dest) const {
        if (len == 0) return EMPTY;

        dest = buf[start];
        return OK;
    }
};
