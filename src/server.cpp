#include <iostream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <algorithm>

// Структура для зберігання інформації про файли
struct FileInfo {
    std::string name;
    std::uintmax_t size;
    std::time_t creation_time;
};

// Структура для кешування результатів
struct CacheEntry {
    std::chrono::steady_clock::time_point timestamp;
    std::vector<FileInfo> files;
};

// Функція для отримання інформації про файли
std::vector<FileInfo> getFilesInfo(const std::string& directory, const std::string& extension) {
    std::vector<FileInfo> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            FileInfo fileInfo;
            fileInfo.name = entry.path().filename().string();
            fileInfo.size = entry.file_size();
            fileInfo.creation_time = std::chrono::system_clock::to_time_t(entry.last_write_time());
            files.push_back(fileInfo);
        }
    }
    return files;
}

// Функція для перевірки, чи кеш є актуальним (5 секунд)
bool isCacheValid(const CacheEntry& cache) {
    auto now = std::chrono::steady_clock::now();
    return (std::chrono::duration_cast<std::chrono::seconds>(now - cache.timestamp).count() < 5);
}

// Функція для обробки запиту клієнта
std::string handleRequest(const std::string& directory, const std::string& extension, std::unordered_map<std::string, CacheEntry>& cache) {
    // Ключ для кешу — поєднання директорії і розширення
    std::string cacheKey = directory + ":" + extension;

    // Перевіряємо, чи є результат у кеші і чи він актуальний
    if (cache.find(cacheKey) != cache.end() && isCacheValid(cache[cacheKey])) {
        std::cout << "Using cached data for: " << cacheKey << std::endl;
        return "Cached result with " + std::to_string(cache[cacheKey].files.size()) + " files.\n";
    }

    // Якщо кеш не актуальний, отримуємо нові дані
    auto files = getFilesInfo(directory, extension);

    // Оновлюємо кеш
    cache[cacheKey] = { std::chrono::steady_clock::now(), files };

    // Формуємо відповідь
    std::string response = "Files found: " + std::to_string(files.size()) + "\n";
    for (const auto& file : files) {
        response += file.name + " | Size: " + std::to_string(file.size) + " | Created: " + std::asctime(std::localtime(&file.creation_time));
    }
    return response;
}

int main() {
    // Ініціалізація серверного сокету
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Створення сокету
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    // Налаштування адреси і порту
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // Прив'язка сокета до порту
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));

    // Початок прослуховування
    listen(server_fd, 3);
    std::cout << "Server is listening on port 8080..." << std::endl;

    // Кеш для зберігання результатів
    std::unordered_map<std::string, CacheEntry> cache;

    while (true) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        char buffer[1024] = {0};
        read(new_socket, buffer, 1024);

        // Обробка запиту (очікуємо, що запит містить директорію і розширення)
        std::string request(buffer);
        std::cout << "Received request: " << request << std::endl;

        // Розділяємо запит на директорію і розширення (очікується формат: "directory:extension")
        std::string directory = request.substr(0, request.find(':'));
        std::string extension = request.substr(request.find(':') + 1);

        // Генеруємо відповідь
        std::string response = handleRequest(directory, extension, cache);

        // Відправляємо відповідь клієнту
        send(new_socket, response.c_str(), response.length(), 0);

        // Закриваємо сокет для клієнта
        close(new_socket);
    }
    return 0;
}

