#pragma once

#include <algorithm>
#include <cassert>
#include "queue.h"

enum class PriorityQueueErr : uint8_t { OK, FULL, EMPTY, BAD_PRIORITY };

template <class T, unsigned int N>
class PriorityQueue {
    struct Element {
        size_t priority;
        T data;

        Element() : priority(0) {}
        Element(T data, size_t priority) : priority(priority), data(data) {}

        bool operator<(const Element& other) const {
            return priority < other.priority;
        }
    };

    Element arr[N];
    size_t len;

   public:
    PriorityQueue() : len(0) {}

    bool is_empty() const { return len == 0; }

    PriorityQueueErr push(T t, size_t priority) {
        if (len >= N) return PriorityQueueErr::FULL;

        len++;

        Element* first = &arr[0];
        Element* last = &arr[len];

        arr[len - 1] = Element(t, priority);

        std::push_heap(first, last);

        return PriorityQueueErr::OK;
    }

    PriorityQueueErr pop(T& dest) {
        if (len == 0) return PriorityQueueErr::EMPTY;

        Element* first = &arr[0];
        Element* last = &arr[len];

        std::pop_heap(first, last);

        dest = arr[len - 1].data;
        len--;

        return PriorityQueueErr::OK;
    }
};
