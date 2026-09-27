#include <sys/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "ThreadPool.hpp"
#include "client_handler.hpp"
#include "Semaphore.hpp"
#include "transfer_registry.hpp"
#include <atomic>
#include <csignal>
// Global atomic flag to indicate shutdown
std::atomic<bool> shuttingDown{false};
// Signal handler to set the shutdown flag
void handleSignal(int signum) {
    shuttingDown.store(true);
}
// Function to log stats periodically
void statsLogger(Stats& stats, std::atomic<bool>& shuttingDown) {
    while (!shuttingDown.load(std::memory_order_relaxed)) {
        std::cout << "[STATS] active=" << stats.activeTransferCount.load()
                   << " totalBytes=" << stats.totalBytesTransferred.load()
                   << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    // Signal handling setup 
    struct sigaction sa{}; 
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // 1. Create the socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    // Let us restart the server quickly during testing
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind to a local address + port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(9000);
   
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    // 3. Start listening — backlog bumped up since multiple clients
    // can now pile up while workers are busy
    if (listen(listen_fd, 128) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    std::cout << "Server listening on port 9000...\n"; 

    // 4. Create the thread pool 
    unsigned int num_threads = std::thread::hardware_concurrency();
    if(num_threads == 0) {
        num_threads = 8; // default to 8 if hardware_concurrency cannot determine
    }
    TransferRegistry registry; 
    Stats s;
    Semaphore sm(5);
    ThreadPool pool(num_threads); 
    // i intentionally declared the ThreadPool object after the TransferRegistry and Semaphore objects because objects are destroyed in the reverse order of their creation. This ensures that the ThreadPool is destroyed first, allowing all worker threads to finish their tasks and exit cleanly before the TransferRegistry and Semaphore objects are destroyed. this is very important because as you wil see after a couple of lines, when we submit a thread to the pool, it will call handleClient, which will use the TransferRegistry and Semaphore objects. If the ThreadPool were destroyed after the TransferRegistry and Semaphore objects, it could lead to undefined behavior or crashes if a worker thread tries to access these objects after they have been destroyed
    std::thread logger(statsLogger, std::ref(s), std::ref(shuttingDown));

    // make a dedicated thread that keeps looping until shutdown is requested, then closes the listening socket
    std::thread shutdownWatcher([&]() {
    while (!shuttingDown.load()) {
        // it sleeps for a short time to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    close(listen_fd);
    });

    // 5. Accept loop —
    // check the global atomic flag to see if we should exit the loop or not
    while (!shuttingDown.load()) {
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            // deffrentiate between a real error and a shutdown request
            if (shuttingDown.load()) break; // if its a shutdown request, break the loop
            perror("accept");
            continue; // don't crash the whole server on one bad accept
        }

        pool.submit([client_fd, &registry, &sm, &s]() {
            handleClient(client_fd, registry, sm, s);
        });
    }
    // we must join the shutdownWatcher thread and the logger thread to ensure it has completed before we exit main
    shutdownWatcher.join();
    logger.join();
    return 0;

    // at the end of main, the thread pool object will go out of scope and its destructor will be called, which will wait for all worker threads to finish working on their current tasks before exiting. This ensures that all client connections are handled cleanly before the server shuts down and prevents any incomplete transfers or resource leaks. The TransferRegistry and Semaphore objects will also be cleaned up properly as they go out of scope, ensuring that all resources are released appropriately.
}