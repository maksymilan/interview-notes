#include <algorithm>
#include <mutex>
#include <cstddef>
#include <memory>
#include <atomic>
#include <vector>

// the data infromation struct
struct MarketData {
    size_t symbol_id;
    double price;
    size_t timestamp;
};

class Subscriber : public std::enable_shared_from_this<Subscriber> {
    public:
        virtual ~Subscriber() = default;
        virtual void on_market_data(const MarketData& data) = 0;
        virtual int get_id() const = 0;
};

// define Subscriber pointer type
using SubscriberSharedPtr = std::shared_ptr<Subscriber>;
using SubscriberWeakPtr = std::weak_ptr<Subscriber>;

class Publisher {
    private:
        using SubList = std::vector<std::weak_ptr<Subscriber>>;
        using SubListPtr = std::shared_ptr<SubList>;
        std::atomic<SubListPtr> subscriber_list_;
        std::mutex write_mtx_;

    public:
        // record subscriber
        void subscribe(std::shared_ptr<Subscriber> sub) {
            std::lock_guard<std::mutex> guard(write_mtx_);

            // mutex has already guranteed the read sequnce
            auto current_list = subscriber_list_.load(std::memory_order_relaxed); 

            // copy the list
            auto new_list = std::make_shared<SubList>(*current_list);

            // remove the unsubscribed subscribers
            new_list->erase(std::remove_if(new_list->begin(), new_list->end(), [](const std::weak_ptr<Subscriber>& wp){
                return wp.expired();
            }), new_list->end());

            // push new subscriber
            new_list->push_back(sub);
            subscriber_list_.store(new_list, std::memory_order_release);
        }

        // remove a subscriber
        void unsubscribe(std::shared_ptr<Subscriber> sub) {
            std::lock_guard<std::mutex> guard(write_mtx_);
            auto current_list = subscriber_list_.load(std::memory_order_relaxed);

            auto new_list = std::make_shared<SubList>(*current_list);
            new_list->erase(std::remove_if(new_list->begin(), new_list->end(), [&](const std::weak_ptr<Subscriber> & wp){
                auto sp = wp.lock();
                return wp.expired() || sp == sub;
            }), new_list->end());

            subscriber_list_.store(new_list, std::memory_order_release);
        }

        // publish a market data
        void publish(const MarketData& data) {
            auto snapshot = subscriber_list_.load(std::memory_order_acquire);
            for (const auto& wp : *snapshot) {
                if (auto sp = wp.lock()) {
                    sp->on_market_data(data);
                }
            }
        }
};
