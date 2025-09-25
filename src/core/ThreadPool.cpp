#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_thread, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit_detached(std::function<void()> f, TaskPriority priority, const std::string& name) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_) {
            return; // Don't submit tasks after shutdown
        }
        tasks_.emplace(std::move(f), priority, name);
    }
    condition_.notify_one();
}

ThreadPool::Stats ThreadPool::get_stats() const {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);

    Stats stats;
    stats.active_threads = active_threads_.load();
    stats.completed_tasks = completed_tasks_.load();
    stats.failed_tasks = failed_tasks_.load();

    // Get queue size without locking (approximate)
    {
        std::lock_guard<std::mutex> queue_lock(const_cast<std::mutex&>(queue_mutex_));
        stats.queued_tasks = tasks_.size();
    }

    return stats;
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPool::worker_thread() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty()) {
                break;
            }

            if (!tasks_.empty()) {
                task = std::move(const_cast<Task&>(tasks_.top()));
                tasks_.pop();
            } else {
                continue;
            }
        }

        // Execute the task
        active_threads_++;
        try {
            task.function();
            completed_tasks_++;
        } catch (const std::exception& e) {
            std::cerr << "Task '" << task.name << "' failed with exception: " << e.what() << std::endl;
            failed_tasks_++;
        } catch (...) {
            std::cerr << "Task '" << task.name << "' failed with unknown exception" << std::endl;
            failed_tasks_++;
        }
        active_threads_--;
    }
}

// ServerTaskManager implementation
ServerTaskManager::ServerTaskManager(size_t worker_threads)
    : thread_pool_(worker_threads) {
}

ServerTaskManager::~ServerTaskManager() {
    shutdown();
}

std::future<void> ServerTaskManager::submit_chunk_generation(std::function<void()> task) {
    return thread_pool_.submit_priority(TaskPriority::NORMAL, "chunk_generation", std::move(task));
}

std::future<void> ServerTaskManager::submit_chunk_save(std::function<void()> task) {
    return thread_pool_.submit_priority(TaskPriority::LOW, "chunk_save", std::move(task));
}

std::future<void> ServerTaskManager::submit_file_io(std::function<void()> task) {
    return thread_pool_.submit_priority(TaskPriority::LOW, "file_io", std::move(task));
}

std::future<void> ServerTaskManager::submit_world_save(std::function<void()> task) {
    return thread_pool_.submit_priority(TaskPriority::HIGH, "world_save", std::move(task));
}

void ServerTaskManager::submit_network_task(std::function<void()> task) {
    thread_pool_.submit_detached(std::move(task), TaskPriority::HIGH, "network_task");
}

std::future<void> ServerTaskManager::submit_game_task(std::function<void()> task) {
    return thread_pool_.submit_priority(TaskPriority::CRITICAL, "game_logic", std::move(task));
}