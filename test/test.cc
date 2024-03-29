#include <cassert>
#include <iostream>
#include <optional>

#include "common/opt_array.h"
#include "common/priority_queue.h"
#include "common/queue.h"

void test_queue() {
    Queue<int, 10> q;

    assert(q.is_empty());
    assert(q.size() == 0);
    assert(q.available() == 10);

    q.push_back(1);
    q.push_back(2);
    q.push_back(3);

    assert(!q.is_empty());
    assert(q.size() == 3);
    assert(q.available() == 7);

    assert(*q.peek_front() == 1);

    assert(q.pop_front() == 1);
    assert(q.pop_front() == 2);
    assert(q.pop_front() == 3);

    assert(q.is_empty());
    assert(q.size() == 0);

    assert(q.peek_front() == nullptr);
    assert(q.pop_front() == std::nullopt);

    for (int i = 0; i < 10; i++) {
        assert(q.push_back(100 + i) == QueueErr::OK);
    }

    assert(q.available() == 0);
    assert(q.push_back(42) == QueueErr::FULL);

    assert(*q.peek_front() == 100);

    q.clear();

    assert(q.is_empty());
    assert(q.pop_front() == std::nullopt);

    assert(q.push_back(1) == QueueErr::OK);
    assert(q.size() == 1);
}

void test_priority_queue() {
    PriorityQueue<int, 10> pq;

    assert(pq.is_empty());

    assert(pq.push(100, 1) == PriorityQueueErr::OK);
    assert(pq.push(200, 2) == PriorityQueueErr::OK);
    assert(pq.push(101, 1) == PriorityQueueErr::OK);

    assert(pq.pop() == 200);
    assert(pq.pop() == 100);
    assert(pq.pop() == 101);
    assert(pq.pop() == std::nullopt);

    pq.push(0, 4);
    pq.push(1, 3);
    pq.push(2, 3);
    pq.push(3, 5);

    assert(*pq.peek() == 3);
    assert(pq.pop() == 3);
    pq.push(3, 5);
    assert(pq.pop() == 3);

    assert(pq.pop() == 0);
    assert(pq.pop() == 1);
    pq.push(1, 3);
    assert(pq.pop() == 2);
    assert(pq.pop() == 1);
}

void test_opt_array() {
    OptArray<char, 8> arr;
    assert(arr.num_present() == 0);

    arr.put('a', 0);
    assert(arr.num_present() == 1);
    assert(arr.has(0));
    assert(arr.get(0) == 'a');
    assert(arr[0] == 'a');

    arr.put('b', 1);
    assert(arr.num_present() == 2);
    assert(arr.has(1));
    assert(arr.get(1) == 'b');
    assert(arr[1] == 'b');

    assert(!arr.has(2));
    arr.put('A', 0);
    assert(arr.num_present() == 2);
    assert(arr.get(0) == 'A');

    assert(arr.take(0) == 'A');
    assert(arr.num_present() == 1);
    assert(!arr.has(0));
    assert(!arr.get(0).has_value());
}

int main() {
    test_queue();
    test_priority_queue();
    test_opt_array();

    std::cout << "unit tests passed" << std::endl;
}
