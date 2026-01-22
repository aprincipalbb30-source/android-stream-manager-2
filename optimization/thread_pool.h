#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <chrono>

namespace AndroidStreamManager {

struct ThreadPoolStats {
    size_t totalThreads;
    size_t activeThreads;
    size_t queuedTasks;
    uint64_t totalTasksProcessed;
    uint64_t failedTasks;
    std::chrono::microseconds averageTaskTime;
    std::chrono::microseconds minTaskTime;
    std::chrono::microseconds maxTaskTime;

    ThreadPoolStats()
        : totalThreads(0)
        , activeThreads(0)
        , queuedTasks(0)
        , totalTasksProcessed(0)
        , failedTasks(0)
        , averageTaskTime(0)
        , minTaskTime(0)
        , maxTaskTime(0) {}
};

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Não copiable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueue task
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    // Controle
    void shutdown();
    void waitForAllTasks();
    bool isIdle() const;

    // Configuração
    void setMaxThreads(size_t maxThreads);

    // Estatísticas
    size_t getActiveThreads() const;
    size_t getQueuedTasks() const;
    ThreadPoolStats getStatistics() const;

private:
    // Worker thread function
    void workerThread(size_t threadId);
    void executeTask(std::function<void()> task, size_t threadId);

    // Workers
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    // Sincronização
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable waitCondition_;
    bool stop_;

    // Estatísticas
    mutable std::mutex statsMutex_;
    std::atomic<size_t> activeThreads_;
    uint64_t totalTasksProcessed_;
    uint64_t failedTasks_;
    std::chrono::microseconds totalTaskTime_;
    std::chrono::microseconds minTaskTime_;
    std::chrono::microseconds maxTaskTime_;
};

// Template implementation
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {

    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queueMutex_);

        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        tasks_.emplace([task]() { (*task)(); });
    }

    condition_.notify_one();
    return res;
}

} // namespace AndroidStreamManager

#endif // THREAD_POOL_H