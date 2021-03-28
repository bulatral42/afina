#include <afina/concurrency/Executor.h>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <iostream>

namespace Afina {
namespace Concurrency {


Executor::Executor(std::string name, std::size_t low_watermark, std::size_t high_watermark, 
                   std::size_t max_queue_size, std::size_t idle_time) :
                   _idle_time{idle_time}, _max_queue_size{max_queue_size}, 
                   _low_watermark{low_watermark}, _high_watermark{high_watermark}, 
                   state{State::kStopped}, _name{name} {}

Executor::~Executor() {
    Stop(true);
}

void Executor::Start() {
    std::unique_lock<std::mutex> _lock(mutex);
    if (state == State::kRun) {
        return;
    }
    while (state != State::kStopped) { // Case ThreadPool was running before
        stop_cond.wait(_lock);
    }
    for (std::size_t i = 0; i < _low_watermark; ++i) {
        std::thread new_thread(ExecuteFunctions::perform, this);
        new_thread.detach();
    }
    all_threads = free_threads = _low_watermark;
    state = State::kRun;
}

void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> _lock(mutex);
    if (state == State::kStopped) {
        return;
    }
    if (state == State::kRun) {
        if (all_threads == 0) {
            state = State::kStopped;
        } else {
            state = State::kStopping;
            new_tasks.notify_all(); // to skip idle_time
        }
    }
    if (await) {
        while (state != State::kStopped) {
            stop_cond.wait(_lock);
        }
    }
}

void ExecuteFunctions::perform(Executor *executor) {
    using State = Afina::Concurrency::Executor::State;
    std::unique_lock<std::mutex> _lock(executor->mutex);
    executor->free_threads += 1;
    bool exit_flag{false};
    while (!executor->tasks.empty() || executor->state == State::kRun) {
        auto to_wait = std::chrono::system_clock::now() + executor->_idle_time;
        while (executor->tasks.empty() && executor->state == State::kRun) {
            if (executor->new_tasks.wait_until(_lock, to_wait) == std::cv_status::timeout &&
                    executor->all_threads > executor->_low_watermark) {
                exit_flag = true;
                break;
            } 
        }
        if (exit_flag || (executor->tasks.empty() && executor->state == State::kStopping)) {
            break;
        }
        
        // Current task
        auto task = executor->tasks.front();
        executor->tasks.pop_front();
        executor->free_threads -= 1;
        _lock.unlock();
        task();
        _lock.lock();
        executor->free_threads += 1;
    } // while

    // Thread is dying
    executor->free_threads -= 1;
    if (--executor->all_threads == 0 && executor->state == State::kStopping) {
        // This is last thread
        executor->state = State::kStopped;
        executor->stop_cond.notify_all();
    }
}


} // namespace Concurrency
} // namespace Afina

