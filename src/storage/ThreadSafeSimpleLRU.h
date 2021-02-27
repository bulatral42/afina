#ifndef AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
#define AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H

#include <cstdlib>
#include <iterator>
#include <map>
#include <mutex>
#include <string>

#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

/**
 * # SimpleLRU thread safe version
 *
 *
 */
class ThreadSafeSimplLRU : public SimpleLRU {
public:
    ThreadSafeSimplLRU(size_t max_size = 1024) : SimpleLRU(max_size) {}
    ~ThreadSafeSimplLRU() {}

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> lock(thread_safe);
        std::srand(std::time(nullptr));
        int delay = std::rand() % 1500;
        std::cout << "Thread # " << std::this_thread::get_id() << " sleeps for ";
        std::cout << delay << " microseconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        return SimpleLRU::Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> lock(thread_safe);
        std::srand(std::time(nullptr));
        int delay = std::rand() % 1500;
        std::cout << "Thread # " << std::this_thread::get_id() << " sleeps for ";
        std::cout << delay << " microseconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        return SimpleLRU::PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> lock(thread_safe);
        std::srand(std::time(nullptr));
        int delay = std::rand() % 1500;
        std::cout << "Thread # " << std::this_thread::get_id() << " sleeps for ";
        std::cout << delay << " microseconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        return SimpleLRU::Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        std::lock_guard<std::mutex> lock(thread_safe);
        std::srand(std::time(nullptr));
        int delay = std::rand() % 1500;
        std::cout << "Thread # " << std::this_thread::get_id() << " sleeps for ";
        std::cout << delay << " microseconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        return SimpleLRU::Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        std::lock_guard<std::mutex> lock(thread_safe);
        std::srand(std::time(nullptr));
        int delay = std::rand() % 1500;
        std::cout << "Thread # " << std::this_thread::get_id() << " sleeps for ";
        std::cout << delay << " microseconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        return SimpleLRU::Get(key, value);
    }

private:
    std::mutex thread_safe;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
