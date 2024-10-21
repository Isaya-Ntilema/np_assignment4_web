#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm> // Include this header for std::count

void handle_client(int client_socket) {
    char buffer[1024];
    read(client_socket, buffer, sizeof(buffer));
    std::istringstream request(buffer);
    std::string method, path, version;
    request >> method >> path >> version;

    if (method != "GET" && method != "HEAD") {
        std::string response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        write(client_socket, response.c_str(), response.size());
        close(client_socket);
        return;
    }

    if (std::count(path.begin(), path.end(), '/') > 3) {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        write(client_socket, response.c_str(), response.size());
        close(client_socket);
        return;
    }

    if (path == "/") path = "/index.html";
    std::ifstream file("." + path, std::ios::binary);
    if (!file) {
        std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
        write(client_socket, response.c_str(), response.size());
    } else {
        std::string response = "HTTP/1.1 200 OK\r\n\r\n";
        write(client_socket, response.c_str(), response.size());
        if (method == "GET") {
            std::vector<char> file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            write(client_socket, file_content.data(), file_content.size());
        }
    }
    close(client_socket);
}

void start_server(const std::string& ip, int port) {
    int server_socket = socket(AF_INET6, SOCK_STREAM, 0);
    sockaddr_in6 server_addr = {};
    server_addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, ip.c_str(), &server_addr.sin6_addr);
    server_addr.sin6_port = htons(port);

    bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);
    std::cout << "[*] Listening on " << ip << ":" << port << std::endl;

    while (true) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (fork() == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        } else {
            close(client_socket);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <IP> <PORT>" << std::endl;
        return 1;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    start_server(ip, port);
    return 0;
}
