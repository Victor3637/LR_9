#include "ipc_utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include "file_utils.h"

bool sendRequestToServer(const std::string& directory, const std::string& extension, int port, std::string& response) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[2048] = {0};

    // Створюємо сокет
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return false;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported" << std::endl;
        return false;
    }

    // Підключення до сервера
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return false;
    }

    // Формуємо запит у форматі "directory:extension"
    std::string request = directory + ":" + extension;
    send(sock, request.c_str(), request.size(), 0);

    // Отримуємо відповідь від сервера
    read(sock, buffer, 2048);
    response = buffer;

    close(sock);
    return true;
}

std::string handleClientRequest(int client_socket, Cache& cache) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);

    std::string request(buffer);
    std::string directory = request.substr(0, request.find(':'));
    std::string extension = request.substr(request.find(':') + 1);

    std::string cacheKey = directory + ":" + extension;
    std::vector<FileInfo> files;

    // Перевірка кешу
    if (cache.isValid(cacheKey)) {
        files = cache.get(cacheKey);
    } else {
        files = getFilesInfo(directory, extension);
        cache.put(cacheKey, files);
    }

    std::string response = "Files found: " + std::to_string(files.size()) + "\n";
    for (const auto& file : files) {
        response += file.name + " | Size: " + std::to_string(file.size) + " | Created: " + std::asctime(std::localtime(&file.creation_time));
    }

    send(client_socket, response.c_str(), response.size(), 0);
    return response;
}
