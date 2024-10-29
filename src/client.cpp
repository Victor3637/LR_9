#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[2048] = {0};

    // Налаштування сокета сервера
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    while (true) {
        // Створюємо новий сокет для кожного запиту
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "Socket creation error" << std::endl;
            return -1;
        }

        // Підключення до сервера
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed" << std::endl;
            return -1;
        }

        // Запит користувача на директорію і розширення файлів
        std::string directory, extension;
        std::cout << "Enter the directory path (or 'exit' to quit): ";
        std::getline(std::cin, directory);
        if (directory == "exit") break;

        std::cout << "Enter the file extension (e.g., .txt): ";
        std::getline(std::cin, extension);

        // Формуємо запит у форматі: "directory:extension"
        std::string request = directory + ":" + extension;

        // Надсилаємо запит на сервер
        send(sock, request.c_str(), request.size(), 0);

        // Чекаємо на відповідь від сервера
        read(sock, buffer, 2048);

        // Виводимо відповідь сервера
        std::cout << "Response from server:\n" << buffer << std::endl;

        // Закриваємо з'єднання після обробки запиту
        close(sock);
    }

    std::cout << "Client program exited." << std::endl;
    return 0;
}
