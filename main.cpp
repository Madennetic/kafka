#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstdint>
#include <string.h>

int main(int argc, char* argv[]) {
    // Disable output buffering
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket: " << std::endl;
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(server_fd);
        std::cerr << "setsockopt failed: " << std::endl;
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9092);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
        close(server_fd);
        std::cerr << "Failed to bind to port 9092" << std::endl;
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        close(server_fd);
        std::cerr << "listen failed" << std::endl;
        return 1;
    }

    std::cout << "Waiting for a client to connect...\n";

    struct sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len);
    std::cout << "Client connected\n";

    // Buffer to collect received data and send data
    char buffer[1024];
    char* send_buffer = nullptr;
    std::size_t length = 0;

    // Receiving data and checking for disconnect (which is kinda useless atp)
    int receive = recv(client_fd, buffer, sizeof(buffer), 0);
    if (receive < 0) {
      std::cerr << "Client error" << std::endl;
      close(client_fd);
    }
    if (receive == 0) {
      std::cerr << "Client disconnected" << std::endl;
      close(client_fd);
    }

    std::cout << buffer << std::endl << sizeof(buffer) << std::endl;

    // Setting up request header variables
    std::int32_t message_size = htonl(sizeof(buffer));
    std::int16_t request_api_key;
    std::int16_t request_api_version;
    std::int32_t correlation_id;

    // Parsing request header variables based on the buffer
    memcpy(&request_api_key, buffer + 4, sizeof(request_api_key));
    memcpy(&request_api_version, buffer + 6, sizeof(request_api_version));
    memcpy(&correlation_id, buffer + 8, sizeof(correlation_id));

    // Setting up response variables
    std::int16_t error_code;

    std::cout << "Key: " << htons(request_api_key) << std::endl;
    std::cout << "Version: " << htons(request_api_version) << std::endl;
    std::cout << "ID: " << htonl(correlation_id) << std::endl;
    std::cout << "Buffer Size: " << htonl(sizeof(buffer)) << std::endl;

    length += sizeof(correlation_id);
    send_buffer = (char*)realloc(send_buffer, length);
    memcpy(send_buffer + length - sizeof(correlation_id), &correlation_id, sizeof(correlation_id));

    // Basic usage of parsed api key and version to get key and see if its the proper version
    if (htons(request_api_key) == 0x12) {
      if (htons(request_api_version) <= 0x04 && htons(request_api_version) >= 0x00) {
        error_code = htons(0);
      }
      else {
        error_code = htons(35);
      }
    }

    length += sizeof(error_code);
    send_buffer = (char*)realloc(send_buffer, length);
    memcpy(send_buffer + length - sizeof(error_code), &error_code, sizeof(error_code));

    // Adding message length to send_buffer and sending it
    send_buffer = (char*)realloc(send_buffer, length + 4);
    memcpy(send_buffer + 4, send_buffer, sizeof(send_buffer));
    std::int32_t len = htonl(length);
    memcpy(send_buffer, &len, 4);
    send(client_fd, send_buffer, length + 4, 0);

    close(client_fd);

    close(server_fd);
    return 0;
}