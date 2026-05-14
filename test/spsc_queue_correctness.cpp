#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

#include "questions/03_SPSC/lockfree_queue_SPSC.cpp"

void TestEmptyAndFull() {
    SPSCQueue<int> queue(2);

    int value = 0;
    assert(!queue.pop(value));
    assert(queue.push(1));
    assert(queue.push(2));
    assert(!queue.push(3));
    assert(queue.pop(value));
    assert(value == 1);
    assert(queue.pop(value));
    assert(value == 2);
    assert(!queue.pop(value));
}

void TestMoveOnlyType() {
    SPSCQueue<std::unique_ptr<int>> queue(4);

    assert(queue.push(std::make_unique<int>(42)));

    std::unique_ptr<int> value;
    assert(queue.pop(value));
    assert(value != nullptr);
    assert(*value == 42);
}

void TestThreadedOrder() {
    constexpr std::uint64_t kItems = 1'000'000;
    SPSCQueue<std::uint64_t> queue(1024);

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < kItems; ++i) {
            while (!queue.push(i)) {
            }
        }
    });

    std::thread consumer([&] {
        for (std::uint64_t expected = 0; expected < kItems; ++expected) {
            std::uint64_t value = 0;
            while (!queue.pop(value)) {
            }
            assert(value == expected);
        }
    });

    producer.join();
    consumer.join();
}

int main() {
    TestEmptyAndFull();
    TestMoveOnlyType();
    TestThreadedOrder();

    std::cout << "SPSC queue correctness tests passed\n";
    return 0;
}
