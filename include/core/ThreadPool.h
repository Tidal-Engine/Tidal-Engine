#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <type_traits>

// Lock-free queue for high-performance inter-thread communication
template<typename T>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(new Node), tail_(head_.load()) {}

    ~LockFreeQueue() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next);
            delete old_head;
        }
    }

    void enqueue(T item) {
        Node* new_node = new Node;
        new_node->data = std::move(item);
        Node* prev_tail = tail_.exchange(new_node);
        prev_tail->next = new_node;
    }

    bool dequeue(T& result) {
        Node* head = head_.load();
        Node* next = head->next;
        if (next == nullptr) {
            return false; // Queue is empty
        }
        result = std::move(next->data);
        head_.store(next);
        delete head;
        return true;
    }

    bool empty() const {
        Node* head = head_.load();
        return head->next == nullptr;
    }

private:
    struct Node {
        std::atomic<Node*> next{nullptr};
        T data;
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};

// Task types for the thread pool
enum class TaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

struct Task {
    std::function<void()> function;
    TaskPriority priority = TaskPriority::NORMAL;
    std::string name; // For debugging/profiling

    Task() = default;
    Task(std::function<void()> func, TaskPriority prio = TaskPriority::NORMAL, const std::string& task_name = "")
        : function(std::move(func)), priority(prio), name(task_name) {}

    // For priority queue (higher priority = higher value)
    bool operator<(const Task& other) const {
        return static_cast<int>(priority) < static_cast<int>(other.priority);
    }
};

// High-performance thread pool for async operations
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Submit a task and get a future for the result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            tasks_.emplace([task]() { (*task)(); }, TaskPriority::NORMAL, "user_task");
        }

        condition_.notify_one();
        return result;
    }

    // Submit a task with priority
    template<typename F, typename... Args>
    auto submit_priority(TaskPriority priority, const std::string& name, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            tasks_.emplace([task]() { (*task)(); }, priority, name);
        }

        condition_.notify_one();
        return result;
    }

    // Submit a fire-and-forget task
    void submit_detached(std::function<void()> f, TaskPriority priority = TaskPriority::NORMAL,
                        const std::string& name = "detached_task");

    // Get thread pool statistics
    struct Stats {
        size_t active_threads = 0;
        size_t queued_tasks = 0;
        size_t completed_tasks = 0;
        size_t failed_tasks = 0;
    };

    Stats get_stats() const;
    void shutdown();
    bool is_shutdown() const { return stop_; }

private:
    std::vector<std::thread> workers_;
    std::priority_queue<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> active_threads_{0};
    std::atomic<size_t> completed_tasks_{0};
    std::atomic<size_t> failed_tasks_{0};

    void worker_thread();
};

// Specialized task queues for different types of work
class ServerTaskManager {
public:
    ServerTaskManager(size_t worker_threads = std::thread::hardware_concurrency());
    ~ServerTaskManager();

    // Chunk-related tasks
    std::future<void> submit_chunk_generation(std::function<void()> task);
    std::future<void> submit_chunk_save(std::function<void()> task);

    // I/O tasks
    std::future<void> submit_file_io(std::function<void()> task);
    std::future<void> submit_world_save(std::function<void()> task);

    // Network tasks (for future networking implementation)
    void submit_network_task(std::function<void()> task);

    // Game logic tasks (high priority)
    std::future<void> submit_game_task(std::function<void()> task);

    // Statistics and monitoring
    ThreadPool::Stats get_stats() const { return thread_pool_.get_stats(); }
    void shutdown() { thread_pool_.shutdown(); }

private:
    ThreadPool thread_pool_;
};

// Message queue for inter-thread communication
template<typename MessageType>
class MessageQueue {
public:
    void push(MessageType msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(msg));
        condition_.notify_one();
    }

    bool pop(MessageType& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        msg = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool wait_and_pop(MessageType& msg, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (condition_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            msg = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<MessageType> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};