#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port> <filename>\n";
        return 1;
    }

    std::string server_ip = argv[1];
    int port = std::stoi(argv[2]);
    std::string filename = argv[3];

    // 1. Create socket and connect
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &addr.sin_addr);

    if (connect(sock_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    // 2. Send the request line
    std::string request = "GET " + filename + "\n";
    send(sock_fd, request.c_str(), request.size(), 0);

    // 3. Read the response header line (OK <size> or ERR ...)
    std::string header;
    char c;
    while (recv(sock_fd, &c, 1, 0) > 0 && c != '\n') {
        header += c;
    }
    std::cout << "Header: " << header << "\n";

    if (header.rfind("OK", 0) != 0) {
        std::cerr << "Server error, aborting.\n";
        close(sock_fd);
        return 1;
    }

    size_t filesize = std::stoul(header.substr(3)); // skip "OK "

    // 4. Read exactly filesize bytes and write to local file
    std::string out_name = "downloaded_" + filename;
    std::ofstream outfile(out_name, std::ios::binary);

    char buffer[65536];
    size_t total_received = 0;
    while (total_received < filesize) {
        ssize_t n = recv(sock_fd, buffer,
                          std::min(sizeof(buffer), filesize - total_received), 0);
        if (n <= 0) break;
        outfile.write(buffer, n);
        total_received += n;
    }

    std::cout << "Received " << total_received << " / " << filesize << " bytes -> "
              << out_name << "\n";

    close(sock_fd);
    return 0;
}