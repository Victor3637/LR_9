#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <vector>
#include "cache.h"

// Функція для отримання інформації про файли з певним розширенням у директорії
std::vector<FileInfo> getFilesInfo(const std::string& directory, const std::string& extension);

#endif // FILE_UTILS_H
