
#include <bit>
#include <cstddef>
#include <new>
#include <type_traits>
#include <atomic>
#include <utility>
#define CACHELINE_SIZE 64

template <typename T>
concept QueueElement =
    std::is_destructible_v<T> && std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

template <QueueElement T>
class SPSCQueue {
private:
    const size_t capacity_;
    const size_t mask_;
    T* slots_;
    struct alignas(CACHELINE_SIZE) ConsumerState {
        std::atomic<size_t> head;
        size_t tail_cache;
    } consumer_ ;
    struct alignas(CACHELINE_SIZE) ProducerState {
        std::atomic<size_t> tail;
        size_t head_cache;
    } producer_ ;

public:
    explicit SPSCQueue(size_t capacity)
        : capacity_(std::bit_ceil(capacity > 1 ? capacity : size_t{2})),
          mask_(capacity_ - 1),
          slots_(static_cast<T*>(
              ::operator new(capacity_ * sizeof(T), std::align_val_t(alignof(T))))),
          consumer_{0, 0},
          producer_{0, 0} {}

    ~SPSCQueue() {
        size_t head = consumer_.head.load(std::memory_order_relaxed);
        size_t tail = producer_.tail.load(std::memory_order_relaxed);
        while (head != tail) {
            slots_[head & mask_].~T();
            ++head;
        }
        ::operator delete(slots_, std::align_val_t(alignof(T)));
    }
    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;

    template<typename... Args>
    bool push(Args&&... args ) {
        size_t tail = producer_.tail.load(std::memory_order_relaxed);
        size_t head = producer_.head_cache;
        // queue is full, check the real head value
        if (tail - head == capacity_) {
            head = consumer_.head.load(std::memory_order_acquire);
            // update cache
            producer_.head_cache = head;
            if (tail - head == capacity_) return false;
        }

        T* dest = &slots_[tail & mask_];
        new (dest) T(std::forward<Args>(args)...);
        producer_.tail.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& out_value) {
        size_t head = consumer_.head.load(std::memory_order_relaxed);
        size_t tail = consumer_.tail_cache;
        if (head == tail) {
            tail = producer_.tail.load(std::memory_order_acquire);
            consumer_.tail_cache = tail;
            if (head == tail) {
                return false;
            }
        }
        T* src = &slots_[head & mask_];
        out_value = std::move(*src);
        src->~T();
        consumer_.head.store(head + 1, std::memory_order_release);
        return true;
    }      
};
