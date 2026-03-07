#include <cstddef>
#include <thread>
#include <vector>
#include <ctime>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "utils/random.h"

struct Task {
    std::vector<Task*> upstream;
    std::vector<Task*> downstream;
    int indegree = 0;
    void run() const {
        int sleep_ms = random_integer(50, 500);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
};

struct Graph {
    std::vector<Task> tasks;
};

class Cluster {
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::queue<const Task*> pending_tasks_;
    std::queue<const Task*> finished_tasks_;
    std::atomic<size_t> total_workers_;
    std::atomic<size_t> available_workers_;
    std::atomic<bool> stop_;
public:
    Cluster(size_t K) : total_workers_(K), available_workers_(K), stop_(false) {
        for (int i = 0; i < K; ++i) {
            workers_.emplace_back([this](){
                while (true) {
                    const Task* t = nullptr;
                    {
                        std::unique_lock<std::mutex> lk(mtx_);
                        cv_.wait(lk, [this](){
                            return  !pending_tasks_.empty() || stop_;
                        });
                        if (stop_ && pending_tasks_.empty()) return ;
                        t = pending_tasks_.front();
                        pending_tasks_.pop();
                    }
                    if (t) {
                        t->run();
                        std::lock_guard<std::mutex> lock_for_push(mtx_);
                        finished_tasks_.push(t);
                    }
                }
            });
        }
    }
    void Stop() {
        for (auto & t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    ~Cluster() {
        stop_ = true;
        cv_.notify_all();
        Stop();
    }
    // Get the total num of worker threads
    int GetAllWorkers() {
        return  total_workers_;
    }

    // Get the num of available worker thread, A worker only be available **after** its finished task being polled
    int GetAvailableWorkers() {
        return  available_workers_;
    }

    // Schedule a task on a worker, return true if success, otherwise false
    bool Schedule(const Task& task) {
        std::lock_guard<std::mutex> lk(mtx_);
        pending_tasks_.push(&task);
        available_workers_--;
        cv_.notify_one();
        return true;
    }

    // Query the finished tasks in the cluster, return a finished task, if any
    // return nullptr if None, Not blocking
    Task* Poll() {
        std::lock_guard<std::mutex> lk(mtx_);
        if(finished_tasks_.empty()) return nullptr;
        auto* t = finished_tasks_.front();
        finished_tasks_.pop();
        available_workers_++;
        return  const_cast<Task*>(t);
    }
};

class Scheduler {
private:
    Graph& graph_;
    Cluster& cluster_;
public:
    Scheduler (Graph& G, Cluster& C) : graph_(G), cluster_(C){};
    void Run() {
        if (graph_.tasks.empty()) return;
        size_t finished_tasks = 0;
        std::queue<Task*> ready_queue;
        for (auto& task : graph_.tasks) {
            task.indegree = task.upstream.size();
            if (task.indegree == 0) {
                ready_queue.push(&task);
            }
        }
        while (finished_tasks != graph_.tasks.size()) {
            bool do_somting = false;
            while (!ready_queue.empty() && cluster_.GetAvailableWorkers() > 0) {
                if(cluster_.Schedule(*ready_queue.front())) {
                    do_somting = true;
                    ready_queue.pop();
                } else {
                    break;
                }
            }
            auto* t = cluster_.Poll();
            if (t != nullptr) {
                finished_tasks++;
                do_somting = true;
                for (auto down_task : t->downstream) {
                    down_task->indegree--;
                    if(down_task->indegree == 0) {
                        ready_queue.push(down_task);
                    }
                }
            }
            if (!do_somting) {
                std::this_thread::yield();
            }
        }
    }
};