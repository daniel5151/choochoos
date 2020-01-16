#pragma once

#include <cassert>
#include "queue.h"

enum class PriorityQueueErr : uint8_t { OK, FULL, EMPTY, BAD_PRIORITY };

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
        if (priority >= MAX_PRIORITY) return PriorityQueueErr::BAD_PRIORITY;

        switch (queues[priority].push_back(t)) {
            case QueueErr::OK:
                return PriorityQueueErr::OK;
            case QueueErr::FULL:
                return PriorityQueueErr::FULL;
            default:
                assert(false);
        }
    }

    PriorityQueueErr pop(T& dest) {
        for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
            if (!queues[i].is_empty()) {
                queues[i].pop_front(dest);
                return PriorityQueueErr::OK;
            }
        }
        return PriorityQueueErr::EMPTY;
    }
};
