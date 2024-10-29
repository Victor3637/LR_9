#include "file_utils.h"
#include <filesystem>

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
