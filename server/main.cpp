#include <sys/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "ThreadPool.hpp"

std::string readLine(int fd) {
    std::string line;
    char c;
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) break;
        if (c == '\n') break;
        line += c;
    }
    return line;
}

bool sendAll(int fd, const char* data, size_t len) {
    size_t totalSent = 0;
    while (totalSent < len) {
        ssize_t n = send(fd, data + totalSent, len - totalSent, 0);
        if (n <= 0) {
            return false; // error or connection closed
        }
        totalSent += n;
    }
    return true;
}
void handleClient(int client_fd) {
     std::cout << "[Thread " << std::this_thread::get_id()
              << "] handling client fd=" << client_fd << "\n";

    std::cout << "Client connected!\n";

    std::string request = readLine(client_fd);

    std::string command, filename;
    size_t spacePos = request.find(' ');
    if (spacePos != std::string::npos) {
        command = request.substr(0, spacePos);
        filename = request.substr(spacePos + 1);
    }

    std::cout << "Command: [" << command << "], Filename: [" << filename << "]\n";

    if (command != "GET" || filename.empty()) {
        std::string err = "ERR bad request\n";
        sendAll(client_fd, err.c_str(), err.size());
        close(client_fd);
        return;
    }

    std::filesystem::path filePath =
        std::filesystem::path("shared_files") / filename;

    if (!std::filesystem::exists(filePath) ||
        !std::filesystem::is_regular_file(filePath)) {
        std::string err = "ERR file not found\n";
        sendAll(client_fd, err.c_str(), err.size());
        close(client_fd);
        return;
    }

    uintmax_t fileSize = std::filesystem::file_size(filePath);

    std::string header = "OK " + std::to_string(fileSize) + "\n";
    if (!sendAll(client_fd, header.c_str(), header.size())) {
        close(client_fd);
        return;
    }

    std::ifstream file(filePath, std::ios::binary);
    char buffer[65536];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytesRead = file.gcount();

        if (!sendAll(client_fd, buffer, static_cast<size_t>(bytesRead))) {
            std::cerr << "Send failed mid-transfer\n";
            break;
        }
    }

    close(client_fd);
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

    // 5. Accept loop —
    while (true) {
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            perror("accept");
            continue; // don't crash the whole server on one bad accept
        }

        pool.submit([client_fd]() {
            handleClient(client_fd);
        });
    }

    close(listen_fd);
    return 0;
}