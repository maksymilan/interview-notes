#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>

#include "questions/03_SPSC/lockfree_queue_SPSC.cpp"

int main(int argc, char** argv) {
    const std::uint64_t item_count =
        argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 50'000'000ULL;
    const std::size_t capacity =
        argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1 << 16;

    SPSCQueue<std::uint64_t> queue(capacity);
    std::atomic<bool> start{false};

    std::thread producer([&] {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (std::uint64_t i = 0; i < item_count; ++i) {
            while (!queue.push(i)) {
            }
        }
    });

    std::uint64_t checksum = 0;
    std::thread consumer([&] {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (std::uint64_t i = 0; i < item_count; ++i) {
            std::uint64_t value = 0;
            while (!queue.pop(value)) {
            }
            checksum += value;
        }
    });

    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - begin).count();
    const double throughput = static_cast<double>(item_count) / seconds;
    const double ns_per_item = seconds * 1e9 / static_cast<double>(item_count);

    std::cout << "items: " << item_count << '\n'
              << "capacity: " << capacity << '\n'
              << "seconds: " << std::fixed << std::setprecision(6) << seconds << '\n'
              << "throughput: " << std::setprecision(2) << throughput << " ops/sec\n"
              << "latency: " << ns_per_item << " ns/item\n"
              << "checksum: " << checksum << '\n';

    return 0;
}
