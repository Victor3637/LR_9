#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <string>
#include <vector>
#include "cache.h"

// Функція для відправки запиту від клієнта на сервер
bool sendRequestToServer(const std::string& directory, const std::string& extension, int port, std::string& response);

// Функція для обробки запиту на сервері та повернення результату клієнту
std::string handleClientRequest(int client_socket, Cache& cache);

#endif // IPC_UTILS_H
