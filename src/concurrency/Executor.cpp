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
                   _name{name}, _low_wm{low_watermark}, _high_wm{high_watermark}, 
                   _max_q{max_queue_size}, _idle_t{idle_time}, state{State::kStopped} {}

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
    for (std::size_t i = 0; i < _low_wm; ++i) {
        std::thread new_thread(perform, this);
        new_thread.detach();
    }
    all_threads = free_threads = _low_wm;
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

void perform(Executor *executor) {
    using State = Afina::Concurrency::Executor::State;
    while (true) {

        std::unique_lock<std::mutex> _lock(executor->mutex);
        if (executor->tasks.empty()) {
            if (executor->state == State::kStopping) {
                break; // All job is finished, thread dying
            }
            if (executor->new_tasks.wait_for(_lock, executor->_idle_t) == 
                    std::cv_status::timeout && executor->all_threads > executor->_low_wm) {
                break; // Don't need this thread, thread dying
            } else {
                // No tasks under low_watermark or 
                // woke up new tasks came up or
                // Stopping process started
                continue;
            }
        }
        // Current task
        auto task = executor->tasks.front();
        executor->tasks.pop_front();
        executor->free_threads -= 1;

        _lock.unlock();
        try {
            task();
        } catch (std::exception &ex) {
            std::cerr << "Error while performing task in Executor: ";
            std::cerr << executor->_name << std::endl;
            std::cerr << "what(): " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "Error while performing task in Executor: ";
            std::cerr << executor->_name << std::endl;
            //std::abort();
        }
        _lock.lock();
        executor->free_threads += 1;
    } // while

    // Thread is dying
    executor->free_threads -= 1;
    if (--(executor->all_threads) == 0 && executor->state == State::kStopping) {
        // This is last thread
        executor->state = State::kStopped;
        executor->stop_cond.notify_all();
    }
}


} // namespace Concurrency
} // namespace Afina

