#pragma once

#include <algorithm>
#include <cassert>
#include <optional>

enum class PriorityQueueErr : uint8_t { OK, FULL };

template <class T, unsigned int N>
class PriorityQueue {
    struct Element {
        int priority;

        // Each element is issued a monotonically increasing 'ticket', to break
        // ties when priorities are equal. An element with a lower 'ticket' is
        // considered higher priority than an element with equal priority but
        // a higher ticket. This helps preserve FIFO within a given priority.
        size_t ticket;

        std::optional<T> data;

        Element() = default;
        Element(T data, int priority, size_t ticket)
            : priority(priority), ticket(ticket), data(data) {}

        bool operator<(const Element& other) const {
            if (priority == other.priority) {
                return ticket > other.ticket;
            }
            return priority < other.priority;
        }
    };

    Element arr[N];
    size_t len;
    size_t ticket_counter;

   public:
    PriorityQueue() : len(0), ticket_counter(0) {}

    bool is_empty() const { return len == 0; }
    size_t size() const { return len; }

    PriorityQueueErr push(T t, int priority) {
        if (len >= N) return PriorityQueueErr::FULL;

        len++;

        Element* first = &arr[0];
        Element* last = &arr[len];

        arr[len - 1] = Element(t, priority, ticket_counter++);

        std::push_heap(first, last);

        return PriorityQueueErr::OK;
    }

    std::optional<T> pop() {
        if (len == 0) return std::nullopt;

        Element* first = &arr[0];
        Element* last = &arr[len];

        std::pop_heap(first, last);

        T ret = arr[len - 1].data.value();
        len--;

        return ret;
    }

    const T* peek() const {
        if (len == 0) return nullptr;
        return &arr[0].data.value();
    }
};
