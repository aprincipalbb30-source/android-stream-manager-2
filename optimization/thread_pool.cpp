#include "thread_pool.h"
#include <iostream>
#include <algorithm>
#include <chrono>

namespace AndroidStreamManager {

ThreadPool::ThreadPool(size_t numThreads)
    : stop_(false)
    , activeThreads_(0) {

    // Limitar número de threads ao hardware disponível
    size_t hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads == 0) {
        hardwareThreads = 4; // fallback
    }

    numThreads = std::min(numThreads, hardwareThreads * 2); // máximo 2x hardware

    std::cout << "ThreadPool inicializado com " << numThreads << " threads "
              << "(hardware: " << hardwareThreads << ")" << std::endl;

    // Criar threads workers
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this, i]() {
            workerThread(i);
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
    std::cout << "ThreadPool destruído" << std::endl;
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            return; // Já parado
        }
        stop_ = true;
    }

    condition_.notify_all();

    // Aguardar todas as threads terminarem
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
}

std::future<typename std::result_of<
    std::decay_t<std::function<void()>>&()>::type>
ThreadPool::enqueue(std::function<void()> task) {

    using return_type = typename std::result_of<
        std::decay_t<std::function<void()>>&()>::type;

    auto packagedTask = std::make_shared<std::packaged_task<return_type()>>(
        std::move(task));

    std::future<return_type> result = packagedTask->get_future();

    {
        std::unique_lock<std::mutex> lock(queueMutex_);

        if (stop_) {
            throw std::runtime_error("ThreadPool está parado");
        }

        tasks_.emplace([packagedTask]() {
            (*packagedTask)();
        });
    }

    condition_.notify_one();
    return result;
}

size_t ThreadPool::getActiveThreads() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return activeThreads_;
}

size_t ThreadPool::getQueuedTasks() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

ThreadPoolStats ThreadPool::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    ThreadPoolStats stats;
    stats.totalThreads = workers_.size();
    stats.activeThreads = activeThreads_;
    stats.queuedTasks = getQueuedTasks();
    stats.totalTasksProcessed = totalTasksProcessed_;
    stats.averageTaskTime = totalTasksProcessed_ > 0 ?
        totalTaskTime_ / totalTasksProcessed_ : std::chrono::microseconds(0);

    return stats;
}

void ThreadPool::setMaxThreads(size_t maxThreads) {
    // Implementação simplificada - em produção seria mais complexa
    // Requereria parar threads existentes e criar novas

    std::cout << "Aviso: setMaxThreads não implementado completamente" << std::endl;
    std::cout << "Para alterar número de threads, reinicie o ThreadPool" << std::endl;
}

void ThreadPool::waitForAllTasks() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    waitCondition_.wait(lock, [this]() {
        return tasks_.empty() && activeThreads_ == 0;
    });
}

bool ThreadPool::isIdle() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.empty() && activeThreads_ == 0;
}

void ThreadPool::workerThread(size_t threadId) {
    std::cout << "Worker thread " << threadId << " iniciada" << std::endl;

    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            condition_.wait(lock, [this]() {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                break;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task) {
            executeTask(task, threadId);
        }
    }

    std::cout << "Worker thread " << threadId << " finalizada" << std::endl;
}

void ThreadPool::executeTask(std::function<void()> task, size_t threadId) {
    auto startTime = std::chrono::high_resolution_clock::now();

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        activeThreads_++;
    }

    try {
        task();

    } catch (const std::exception& e) {
        std::cerr << "Exceção em worker thread " << threadId << ": " << e.what() << std::endl;

        std::lock_guard<std::mutex> lock(statsMutex_);
        failedTasks_++;

    } catch (...) {
        std::cerr << "Exceção desconhecida em worker thread " << threadId << std::endl;

        std::lock_guard<std::mutex> lock(statsMutex_);
        failedTasks_++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto taskDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        activeThreads_--;
        totalTasksProcessed_++;
        totalTaskTime_ += taskDuration;

        // Atualizar estatísticas de tempo
        if (taskDuration > maxTaskTime_) {
            maxTaskTime_ = taskDuration;
        }
        if (minTaskTime_ == std::chrono::microseconds(0) || taskDuration < minTaskTime_) {
            minTaskTime_ = taskDuration;
        }
    }

    // Notificar que uma tarefa foi concluída
    waitCondition_.notify_all();
}

} // namespace AndroidStreamManager