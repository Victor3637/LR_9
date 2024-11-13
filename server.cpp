#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <thread>
#include <mutex>
#include <fstream>
#include <chrono>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define MAX_FILENAME_LEN 260

#define DIRECTORY1 L"E:\\Операційні системи\\Лабораторні\\Лаб.9\\durektoriya1\\*"
#define DIRECTORY2 L"E:\\Операційні системи\\Лабораторні\\Лаб.9\\durektoriya2\\*"
#define DIRECTORY3 L"E:\\Операційні системи\\Лабораторні\\Лаб.9\\durektoriya3\\*"

std::string cacheData;
std::mutex cacheMutex;
std::ofstream infoFile("information.txt", std::ios::app);
std::chrono::time_point<std::chrono::system_clock> lastCacheTime;
std::atomic<bool> keepRunning(true);

void GetDirectoryInfoByExtension(const wchar_t* dirPath, const char* extension, char* buffer, size_t bufferSize) {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW(dirPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        snprintf(buffer, bufferSize, "Не вдалося знайти каталог.");
        return;
    }

    size_t totalSize = 0;
    char fileList[DEFAULT_BUFLEN] = "";
    SYSTEMTIME creationTime;

    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char fileName[MAX_FILENAME_LEN];
            wcstombs_s(NULL, fileName, findFileData.cFileName, MAX_FILENAME_LEN);
            fileName[MAX_FILENAME_LEN - 1] = '\0';

            if (strlen(fileName) > strlen(extension) &&
                strcmp(fileName + strlen(fileName) - strlen(extension), extension) == 0) {
                strncat_s(fileList, DEFAULT_BUFLEN, fileName, _TRUNCATE);
                strncat_s(fileList, DEFAULT_BUFLEN, "\n", _TRUNCATE);

                LARGE_INTEGER fileSize;
                fileSize.LowPart = findFileData.nFileSizeLow;
                fileSize.HighPart = findFileData.nFileSizeHigh;
                totalSize += fileSize.QuadPart;

                FileTimeToSystemTime(&findFileData.ftCreationTime, &creationTime);
                char dateStr[50];
                snprintf(dateStr, sizeof(dateStr), " - Дата створення: %02d/%02d/%04d %02d:%02d\n",
                    creationTime.wDay, creationTime.wMonth, creationTime.wYear,
                    creationTime.wHour, creationTime.wMinute);
                strncat_s(fileList, DEFAULT_BUFLEN, dateStr, _TRUNCATE);
            }
        }
    } while (FindNextFileW(hFind, &findFileData) != 0);

    FindClose(hFind);

    if (fileList[0] == '\0') {
        snprintf(buffer, bufferSize, "Файли з розширенням %s не знайдено.", extension);
    }
    else {
        snprintf(buffer, bufferSize, "Загальний розмір файлів: %llu байт\nСписок файлів:\n%s", totalSize, fileList);
    }
}

void TryWriteToCache() {
    auto currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsedSeconds = currentTime - lastCacheTime;

    if (elapsedSeconds.count() >= 5 && !cacheData.empty()) {
        FILE* cacheFile;
        fopen_s(&cacheFile, "cash.txt", "a");
        if (cacheFile) {
            fprintf(cacheFile, "%s", cacheData.c_str());
            fclose(cacheFile);
        }
        cacheData.clear();
        lastCacheTime = currentTime;
    }
}

void CacheWriteThread() {
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::lock_guard<std::mutex> lock(cacheMutex);
        TryWriteToCache();
    }
}



int __cdecl main(void) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL;
    struct addrinfo hints = { 0 };

    char recvbuf[DEFAULT_BUFLEN];
    char sendbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    lastCacheTime = std::chrono::system_clock::now();

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup завершився з помилкою: %d\n", iResult);
        return 1;
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo завершився з помилкою: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket завершився з помилкою: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind завершився з помилкою: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen завершився з помилкою: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Сервер очікує підключення...\n");

    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept завершився з помилкою: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    closesocket(ListenSocket);

    std::thread cacheThread(CacheWriteThread);

    while (1) {
        iResult = recv(ClientSocket, recvbuf, recvbuflen - 1, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            printf("Отримано повідомлення: %s\n", recvbuf);

            int directoryChoice = recvbuf[0] - '0';
            const char* extension = recvbuf + 1;

            const wchar_t* dirPath;
            switch (directoryChoice) {
            case 1: dirPath = DIRECTORY1; break;
            case 2: dirPath = DIRECTORY2; break;
            case 3: dirPath = DIRECTORY3; break;
            default:
                snprintf(sendbuf, sizeof(sendbuf), "Неправильний вибір каталогу.");
                send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
                continue;
            }

            GetDirectoryInfoByExtension(dirPath, extension, sendbuf, sizeof(sendbuf));

            std::lock_guard<std::mutex> lock(cacheMutex);
            if (infoFile.is_open()) {
                infoFile << "Запит: " << recvbuf << "\nВідповідь:\n" << sendbuf << "\n\n";
            }

            
            cacheData += sendbuf;
            cacheData += "\n";

            iResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
            if (iResult == SOCKET_ERROR) {
                printf("send завершився з помилкою: %d\n", WSAGetLastError());
            }
        }
        else if (iResult == 0) {
            printf("З'єднання закрито клієнтом\n");
            break;
        }
        else {
            printf("recv завершився з помилкою: %d\n", WSAGetLastError());
            break;
        }
    }

    keepRunning = false;
    cacheThread.join();

    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}





// hdsjfhalkjfhuh48hfaj;oohfhjahkuhugu4fu





