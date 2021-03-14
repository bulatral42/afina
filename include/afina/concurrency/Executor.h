#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <bits/c++config.h>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

namespace Afina {
namespace Concurrency {

void perform(); // Every thread runnig function.

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

    Executor(std::string name, std::size_t low_watermark = 4, std::size_t high_watermark = 8, 
             std::size_t max_queue_size = 64, std::size_t idle_time = 4);
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
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> _lock(this->mutex);
        if (tasks.size() >= _max_q || state.load() != State::kRun) {
            return false;
        }        

        // Enqueue new task
        tasks.push_back(exec);
        if (tasks.size() <= free_threads) {
            new_tasks.notify_all();
        } else if (all_threads < _high_wm) {
            ++free_threads;
            ++all_threads;           
            _lock.unlock();

            std::thread new_thread(perform, this);
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
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor);

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
    std::deque<std::function<void()>> tasks;
    
    std::string _name;
    std::size_t _low_wm, _high_wm, _max_q;
    std::chrono::milliseconds _idle_t;

    std::size_t all_threads, free_threads;

    /**
     * Flag to stop bg threads
     */
    std::atomic<State> state;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
