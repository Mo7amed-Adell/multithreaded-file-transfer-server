#include <sys/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <string>

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

int main() {
    // 1. Create the socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    // 2. Bind to a local address + port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;   // listen on all local interfaces
    addr.sin_port = htons(9000);          // pick a port

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    // 3. Start listening
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port 9000...\n";

    // 4. Accept exactly one client
    int client_fd = accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
        perror("accept");
        return 1;
    }

    std::cout << "Client connected!\n";

    // 5. Read and parse the request line
    std::string request = readLine(client_fd);

    std::string command, filename;
    size_t spacePos = request.find(' ');
    if (spacePos != std::string::npos) {
        command = request.substr(0, spacePos);
        filename = request.substr(spacePos + 1);
    }

    std::cout << "Command: [" << command << "], Filename: [" << filename << "]\n";

    // 6. Look up the file
    std::filesystem::path filePath = std::filesystem::path("shared_files") / filename;

    if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
        std::string err = "ERR file not found\n";
        sendAll(client_fd, err.c_str(), err.size());
        close(client_fd);
        close(listen_fd);
        return 0;
    }

    uintmax_t fileSize = std::filesystem::file_size(filePath);
    std::cout << "File found: " << filePath << " (" << fileSize << " bytes)\n";

    // 7. Send the OK header
    std::string header = "OK " + std::to_string(fileSize) + "\n";
    sendAll(client_fd, header.c_str(), header.size());

    // 8. Stream the file in 64KB chunks
    std::ifstream file(filePath, std::ios::binary);
    char buffer[65536]; // 64KB

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytesRead = file.gcount();
        if (!sendAll(client_fd, buffer, bytesRead)) {
            std::cerr << "Send failed mid-transfer\n";
            break;
        }
    }

    close(client_fd);
    close(listen_fd);
    return 0;
}