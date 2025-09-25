/**
 * @file ThreadPool.h
 * @brief High-performance thread pool with priority scheduling and lock-free queues
 * @author Tidal Engine Team
 * @version 1.0
 */

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

/**
 * @brief Lock-free queue for high-performance inter-thread communication
 *
 * Implements a Michael & Scott lock-free queue algorithm for thread-safe
 * operations without mutex contention. Suitable for producer-consumer patterns
 * in high-throughput scenarios.
 *
 * @tparam T Type of elements to store in the queue
 * @warning This is a lock-free data structure - use with care in concurrent environments
 */
template<typename T>
class LockFreeQueue {
public:
    /**
     * @brief Construct empty lock-free queue
     */
    LockFreeQueue() : head_(new Node), tail_(head_.load()) {}

    /**
     * @brief Destructor - cleans up remaining nodes
     */
    ~LockFreeQueue() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next);
            delete old_head;
        }
    }

    /**
     * @brief Add item to the back of the queue (thread-safe)
     * @param item Item to enqueue
     */
    void enqueue(T item) {
        Node* new_node = new Node;
        new_node->data = std::move(item);
        Node* prev_tail = tail_.exchange(new_node);
        prev_tail->next = new_node;
    }

    /**
     * @brief Remove item from front of queue (thread-safe)
     * @param result Reference to store dequeued item
     * @return True if item was dequeued, false if queue was empty
     */
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

    /**
     * @brief Check if queue is empty (thread-safe)
     * @return True if queue contains no elements
     */
    bool empty() const {
        Node* head = head_.load();
        return head->next == nullptr;
    }

private:
    /**
     * @brief Internal node structure for linked list
     */
    struct Node {
        std::atomic<Node*> next{nullptr};   ///< Atomic pointer to next node
        T data;                             ///< Stored data
    };

    std::atomic<Node*> head_;   ///< Atomic head pointer
    std::atomic<Node*> tail_;   ///< Atomic tail pointer
};

/**
 * @brief Task priority levels for thread pool scheduling
 */
enum class TaskPriority {
    LOW = 0,        ///< Background tasks (I/O, cleanup)
    NORMAL = 1,     ///< Standard game logic tasks
    HIGH = 2,       ///< Time-critical tasks (input, networking)
    CRITICAL = 3    ///< System-critical tasks (must run immediately)
};

/**
 * @brief Task wrapper for thread pool execution
 */
struct Task {
    std::function<void()> function;         ///< Function to execute
    TaskPriority priority = TaskPriority::NORMAL;  ///< Task priority level
    std::string name;                       ///< Task name for debugging/profiling

    /**
     * @brief Default constructor
     */
    Task() = default;

    /**
     * @brief Construct task with function and optional priority/name
     * @param func Function to execute
     * @param prio Task priority level
     * @param task_name Task name for debugging
     */
    Task(std::function<void()> func, TaskPriority prio = TaskPriority::NORMAL, const std::string& task_name = "")
        : function(std::move(func)), priority(prio), name(task_name) {}

    /**
     * @brief Comparison operator for priority queue (higher priority = higher value)
     * @param other Other task to compare against
     * @return True if this task has lower priority than other
     */
    bool operator<(const Task& other) const {
        return static_cast<int>(priority) < static_cast<int>(other.priority);
    }
};

/**
 * @brief High-performance thread pool with priority scheduling
 *
 * Provides a thread pool implementation with the following features:
 * - Priority-based task scheduling
 * - Configurable number of worker threads
 * - Future-based return value handling
 * - Statistics tracking for monitoring
 * - Graceful shutdown with task completion
 *
 * @note Uses std::priority_queue for task ordering
 * @see ServerTaskManager for specialized server task management
 */
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