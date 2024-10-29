#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>

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

class Cache {
public:
    // Метод для перевірки, чи є дані у кеші та чи вони актуальні
    bool isValid(const std::string& key) const;

    // Метод для отримання даних з кешу
    std::vector<FileInfo> get(const std::string& key);

    // Метод для збереження даних у кеш
    void put(const std::string& key, const std::vector<FileInfo>& files);

private:
    // Кеш даних
    std::unordered_map<std::string, CacheEntry> cache_;
};

#endif // CACHE_H
