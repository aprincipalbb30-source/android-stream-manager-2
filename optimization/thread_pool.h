#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

namespace AndroidStreamManager {

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    size_t getQueueSize() const;
    size_t getActiveThreads() const;
    void waitAll();
    
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<size_t> activeThreads;
};

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks.emplace([task]() { (*task)(); });
    }
    
    condition.notify_one();
    return result;
}

class ConnectionPool {
public:
    struct Connection {
        int id;
        std::string host;
        int port;
        std::chrono::system_clock::time_point lastUsed;
        bool inUse;
        std::shared_ptr<SecureTlsClient> client;
    };
    
    ConnectionPool(size_t maxConnections = 10,
                  std::chrono::seconds maxIdleTime = std::chrono::seconds(30));
    ~ConnectionPool();
    
    std::shared_ptr<SecureTlsClient> acquireConnection(
        const std::string& host, int port);
    void releaseConnection(int connectionId);
    
    void cleanupIdleConnections();
    
private:
    std::vector<Connection> connections;
    std::mutex mutex;
    size_t maxConnections;
    std::chrono::seconds maxIdleTime;
    std::atomic<int> nextId{0};
    
    std::shared_ptr<SecureTlsClient> createNewConnection(
        const std::string& host, int port);
};

} // namespace AndroidStreamManager

#endif // THREAD_POOL_H