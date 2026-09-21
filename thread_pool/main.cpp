#include "ThreadPool.hpp"
#include <iostream>
#include <atomic>

int main() {
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    for (int i = 0; i < 500; ++i) {
        pool.submit([&counter] {
            counter++;
        });
    }

    // give the pool time to finish before the destructor runs
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "tasks completed: " << counter << " (expected 500)\n";
}