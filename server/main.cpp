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

void statsLogger(Stats& stats, std::atomic<bool>& running) {
    while (running.load(std::memory_order_relaxed)) {
        std::cout << "[STATS] active=" << stats.activeTransferCount.load()
                   << " totalBytes=" << stats.totalBytesTransferred.load()
                   << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
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
    ThreadPool pool(num_threads); 
    TransferRegistry registry;
    Stats s;
    Semaphore sm(5);
    std::atomic<bool> running{true};
    std::thread logger(statsLogger, std::ref(s), std::ref(running));
    // 5. Accept loop —
    while (true) {
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            perror("accept");
            continue; // don't crash the whole server on one bad accept
        }

        pool.submit([client_fd, &registry, &sm, &s]() {
            handleClient(client_fd, registry, sm, s);
        });
    }

    close(listen_fd);
    running = false;
    logger.join();
    return 0;
}