#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <chrono>

#include <iostream>

namespace Afina {
namespace Concurrency {


class Executor;

namespace ExecuteFunctions {
void perform(Afina::Concurrency::Executor *);
};

/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no new task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };
    
public:

    Executor(std::string name, std::size_t low_watermark = 4, std::size_t high_watermark = 8, 
             std::size_t max_queue_size = 64, std::size_t idle_time = 1000);
    ~Executor();

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);
    void Start();

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void ExecuteFunctions::perform(Executor *executor);
    
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> _lock(this->mutex);
        if (tasks.size() >= _max_queue_size || state != State::kRun) {
            return false;
        }        

        // Enqueue new task
        tasks.push_back(exec);
        if (tasks.size() <= free_threads) {
            new_tasks.notify_all();
        } else if (all_threads < _high_watermark) {
            ++all_threads;           
            _lock.unlock();

            std::thread new_thread(ExecuteFunctions::perform, this);
            new_thread.detach();
        }
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;


    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable new_tasks, stop_cond;

    /**
     * Task queue
     */
    std::deque<std::function<void() noexcept>> tasks;
    
    const std::string _name;
    const std::size_t _low_watermark, _high_watermark, _max_queue_size;
    const std::chrono::milliseconds _idle_time;

    std::size_t all_threads, free_threads;

    /**
     * Flag to stop bg threads
     */
    State state;
};


} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
