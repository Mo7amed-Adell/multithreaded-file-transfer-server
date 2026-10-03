#pragma once
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <optional>
#include <thread>
#include <chrono>
#include "../transfer_registry/transfer_registry.hpp"
#include "../../thread_pool/Semaphore.hpp"


inline std::string readLine(int fd) {
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

inline bool sendAll(int fd, const char* data, size_t len) {
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

inline std::string formatTransferLine(const TransferState& t) {
    return std::to_string(t.id) + " " + t.filename + " " +
           std::to_string(t.bytesSent) + " " + std::to_string(t.totalBytes) +
           " " + statusToString(t.status) + "\n";
}

inline void handleClient(int client_fd, TransferRegistry& rgstry, Semaphore& sm, Stats& stats) {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] handling client fd=" << client_fd << "\n";

    std::cout << "Client connected!\n";

    std::string request = readLine(client_fd);

    std::string command, argument;
    size_t spacePos = request.find(' ');
    if (spacePos != std::string::npos) {
        command = request.substr(0, spacePos);
        argument = request.substr(spacePos + 1);
    }

    std::cout << "Command: [" << command << "], Filename: [" << argument << "]\n";

    if (command == "GET") {
        if (argument.empty()) {
            std::string err = "ERR bad request\n";
            sendAll(client_fd, err.c_str(), err.size());
            close(client_fd);
            return;
        }

        std::filesystem::path filePath =
            std::filesystem::path("shared_files") / argument;

        if (!std::filesystem::exists(filePath) ||
            !std::filesystem::is_regular_file(filePath)) {
            std::string err = "ERR file not found\n";
            sendAll(client_fd, err.c_str(), err.size());
            close(client_fd);
            return;
        }

        uintmax_t fileSize = std::filesystem::file_size(filePath);
        uint64_t transferId = rgstry.create(argument, fileSize);

        std::string header = "OK " + std::to_string(fileSize) + "\n";
        if (!sendAll(client_fd, header.c_str(), header.size())) {
            close(client_fd);
            return;
        }
        SemaphoreGuard g(sm);
        stats.activeTransferCount.fetch_add(1, std::memory_order_relaxed);


        std::ifstream file(filePath, std::ios::binary);
        char buffer[65536];
        bool success = true;
        uint64_t totalSent = 0;
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            std::streamsize bytesRead = file.gcount();
            if (!sendAll(client_fd, buffer, static_cast<size_t>(bytesRead))) {
                std::cerr << "Send failed mid-transfer\n";
                success = false;
                break;
            }
            totalSent += static_cast<uint64_t>(bytesRead);
            rgstry.updateProgress(transferId, totalSent);
            stats.totalBytesTransferred.fetch_add(static_cast<uint64_t>(bytesRead), std::memory_order_relaxed);
             std::this_thread::sleep_for(std::chrono::milliseconds(60)); // Simulate slower transfer for testing we can remove this later
        }
            stats.activeTransferCount.fetch_sub(1, std::memory_order_relaxed);
        if (success) {
            rgstry.setStatus(transferId, TransferStatus::Done);

        } else {
            rgstry.setStatus(transferId, TransferStatus::Error);
        }
        rgstry.remove(transferId);
    }
    else if (command == "STATUS") {
        if (argument == "ALL") {
            std::vector<TransferState> all = rgstry.getAll();
            std::string response;
            for (const auto& t : all) {
                response += formatTransferLine(t);
            }
            sendAll(client_fd, response.c_str(), response.size());
        } else {
            try {
                uint64_t id = std::stoull(argument);
                std::optional<TransferState> result = rgstry.get(id);
                if (result.has_value()) {
                    std::string response = formatTransferLine(result.value());
                    sendAll(client_fd, response.c_str(), response.size());
                } else {
                    std::string err = "ERR transfer not found\n";
                    sendAll(client_fd, err.c_str(), err.size());
                }
            } catch (const std::exception&) {
                std::string err = "ERR bad request\n";
                sendAll(client_fd, err.c_str(), err.size());
            }
        }
    }
    else {
        std::string err = "ERR bad request\n";
        sendAll(client_fd, err.c_str(), err.size());
    }

    close(client_fd);
}