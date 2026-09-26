#pragma once
#include <mutex>
#include <condition_variable>
// A simple counting semaphore implementation using mutex and condition_variable
// The semaphore maintains a count of available resources and allows threads to acquire and release them in a thread-safe manner. When a thread tries to acquire the semaphore and the count is zero, it will block until another thread releases the semaphore, increasing the count.
class Semaphore {
 public:
    Semaphore(int count) : count_(count) {}
    void acquire() {
     std::unique_lock<std::mutex> lock(m_);
     std::cout << "[Thread " << std::this_thread::get_id()
              << "] waiting for slot\n";
     cv_.wait(lock, [this]{
      return count_ > 0;
     });
     count_--;
     std::cout << "[Thread " << std::this_thread::get_id()
              << "] slot acquired\n";
    }
    void release() {
     std::unique_lock<std::mutex> lock(m_);
      count_++;
      lock.unlock();
      cv_.notify_one();
    }
 private:
    int count_;
    std::mutex m_;
    std::condition_variable cv_;
};
// this is just a RAII wrapper for the semaphore, it will acquire the semaphore in the constructor and release it in the destructor, this is useful for ensuring that the semaphore is always released even if an exception is thrown or a return statement is hit it behaves like a lock guard for the semaphore
class SemaphoreGuard {
 public:
   SemaphoreGuard(Semaphore& sm) : sm_(sm) {
   sm_.acquire();
   }
   ~SemaphoreGuard() {
    sm_.release();
   }
 private:
   Semaphore& sm_;

};