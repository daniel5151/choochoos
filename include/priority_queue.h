#pragma once

#include <cassert>
#include "queue.h"

enum PriorityQueueErr { PQ_OK, PQ_FULL, PQ_EMPTY, PQ_BAD_PRIORITY };

template <class T, size_t MAX_PRIORITY, unsigned int N>
class PriorityQueue {
    Queue<T, N> queues[MAX_PRIORITY];

   public:
    PriorityQueue() : queues() {}

    bool is_empty() const {
        for (size_t i = 0; i < MAX_PRIORITY; i++) {
            if (!queues[i].is_empty()) return false;
        }
        return true;
    }

    PriorityQueueErr push(T t, size_t priority) {
        if (priority >= MAX_PRIORITY) return PQ_BAD_PRIORITY;

        switch (queues[priority].push_back(t)) {
            case OK:
                return PQ_OK;
            case FULL:
                return PQ_FULL;
            default:
                assert(false);
        }
    }

    PriorityQueueErr pop(T& dest) {}  // TODO
};
