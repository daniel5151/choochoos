#include <cassert>
#include <iostream>

#include "priority_queue.h"
#include "queue.h"

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

    int x;
    assert(q.peek_front(x) == QueueErr::OK && x == 1);

    assert(q.pop_front(x) == QueueErr::OK && x == 1);
    assert(q.pop_front(x) == QueueErr::OK && x == 2);
    assert(q.pop_front(x) == QueueErr::OK && x == 3);

    assert(q.is_empty());
    assert(q.size() == 0);

    assert(q.peek_front(x) == QueueErr::EMPTY);
    assert(q.pop_front(x) == QueueErr::EMPTY);

    for (int i = 0; i < 10; i++) {
        assert(q.push_back(100 + i) == QueueErr::OK);
    }

    assert(q.available() == 0);
    assert(q.push_back(42) == QueueErr::FULL);

    assert(q.peek_front(x) == QueueErr::OK && x == 100);
}

void test_priority_queue() {
    PriorityQueue<int, 10> pq;

    assert(pq.is_empty());

    assert(pq.push(100, 1) == PriorityQueueErr::OK);
    assert(pq.push(200, 2) == PriorityQueueErr::OK);
    assert(pq.push(101, 1) == PriorityQueueErr::OK);

    int x;
    assert(pq.pop(x) == PriorityQueueErr::OK && x == 200);
    assert(pq.pop(x) == PriorityQueueErr::OK && x == 100);
    assert(pq.pop(x) == PriorityQueueErr::OK && x == 101);
    assert(pq.pop(x) == PriorityQueueErr::EMPTY && x == 101);
}

int main() {
    test_queue();
    test_priority_queue();

    std::cout << "all tests passed" << std::endl;
}
