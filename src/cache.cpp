#include "cache.h"

bool Cache::isValid(const std::string& key) const {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // Перевіряємо, чи кеш зберігається менше 5 секунд
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - it->second.timestamp).count() < 5;
    }
    return false;
}

std::vector<FileInfo> Cache::get(const std::string& key) {
    if (isValid(key)) {
        return cache_.at(key).files;
    }
    return {};
}

void Cache::put(const std::string& key, const std::vector<FileInfo>& files) {
    CacheEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.files = files;
    cache_[key] = entry;
}

