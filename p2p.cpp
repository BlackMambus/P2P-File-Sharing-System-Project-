#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <cstring>
#include <vector>
#include <filesystem>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUFFER_SIZE 1024

namespace fs = std::filesystem;

// Serve files from this directory
const std::string sharedFolder = "./shared";

// Server thread: listens for incoming file requests
void startServer() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket failed");
        return;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 3);

    std::cout << "📡 Server listening on port " << PORT << "...\n";

    while (true) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
        read(new_socket, buffer, BUFFER_SIZE);
        std::string filename(buffer);
        std::string filepath = sharedFolder + "/" + filename;

        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            std::string msg = "ERROR: File not found\n";
            send(new_socket, msg.c_str(), msg.size(), 0);
        } else {
            std::string msg = "OK\n";
            send(new_socket, msg.c_str(), msg.size(), 0);

            while (!file.eof()) {
                file.read(buffer, BUFFER_SIZE);
                send(new_socket, buffer, file.gcount(), 0);
            }
        }

        close(new_socket);
    }
}

// Client function: requests a file from a peer
void requestFile(const std::string& peerIP, const std::string& filename) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, peerIP.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        return;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed\n";
        return;
    }

    send(sock, filename.c_str(), filename.size(), 0);
    read(sock, buffer, BUFFER_SIZE);
    std::string response(buffer);

    if (response.find("OK") != 0) {
        std::cerr << "Server error: " << response;
        close(sock);
        return;
    }

    std::ofstream outFile("downloaded_" + filename, std::ios::binary);
    int bytesRead;
    while ((bytesRead = read(sock, buffer, BUFFER_SIZE)) > 0) {
        outFile.write(buffer, bytesRead);
    }

    std::cout << "✅ File downloaded: downloaded_" << filename << "\n";
    close(sock);
}

int main() {
    fs::create_directory(sharedFolder);

    std::thread serverThread(startServer);
    serverThread.detach();

    std::string command;
    while (true) {
