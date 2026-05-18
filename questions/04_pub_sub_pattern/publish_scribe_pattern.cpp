#include <vector>
#include <mutex>
#include <atomic>

struct  Element {
    size_t element_value_;
    size_t element_id_;    
};

class Consumer {
private:
    size_t consumer_id_;
    size_t element_offset_;
    PubSubService& pub_service_;
public:
    Consumer(size_t id, PubSubService& pub_service) : consumer_id_(id), pub_service_(pub_service) {
        element_offset_ = pub_service_.fetch_offset();
    };
    // consume an element, return true if consume sucessfully, otherwise false
    bool consume() {

    }
};

class Producer {
private:
    PubSubService& pub_service_;
    size_t next_element_id_;
public:
    void produce();
};

class PubSubService {
private:
    std::atomic<size_t> next_consumer_id_;
    std::mutex mtx_;
    std::vector<Element> elements_;
    std::vector<Consumer> consumers_;
public:
    PubSubService() : next_consumer_id_(0) {
        consumers_.reserve(1024);
        elements_.reserve(1024);
    }
    // register a consumer into the consumers_, the consumer only can consume the element after it has been registered
    void register_consumer() {
        size_t consumer_id;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            consumer_id = next_consumer_id_;
            next_consumer_id_++;
        }
        consumers_.emplace_back(consumer_id, *this);
    }
    void register_element(Element& element) {
        std::lock_guard<std::mutex> lk(mtx_);
        elements_.emplace_back(element);
    }
    size_t fetch_offset() {
        std::lock_guard<std::mutex> lk(mtx_);
        return elements_.size();
    }
    ~PubSubService();
};
