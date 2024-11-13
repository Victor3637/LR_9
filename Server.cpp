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
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "27015"
#define MAX_FILENAME_LEN 260
#define MAX_EXTENSION_LEN 16
#define CACHE_RELEVANCY_TIME 5

std::ofstream LogFile("ServerLogs.txt", std::ios::app);

std::string cacheData;
std::mutex cacheMutex;          
std::mutex logMutex;            
std::mutex recvMutex;           
std::mutex sendMutex;           
std::chrono::time_point<std::chrono::system_clock> lastCacheTime;
std::atomic<bool> keepRunning(true);

void GetDirectoryInfoByExtension(const char* dirPath, const char* extension, char* buffer, size_t bufferSize) {
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*.*", dirPath);

    hFind = FindFirstFileA(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        snprintf(buffer, bufferSize, "Не вдалося знайти каталог.");
        return;
    }

    size_t totalSize = 0;
    char fileList[DEFAULT_BUFLEN] = "";
    SYSTEMTIME creationTime;

    do {
        if (findFileData.cFileName[0] == '.' &&
            (findFileData.cFileName[1] == '\0' ||
                (findFileData.cFileName[1] == '.' && findFileData.cFileName[2] == '\0'))) {
            continue;
        }

        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const char* fileName = findFileData.cFileName;

            const char* ext = strrchr(fileName, '.');
            if (strcmp(ext + 1, extension) == 0) {
                char fileNameBuffer[MAX_FILENAME_LEN];
                strcpy_s(fileNameBuffer, sizeof(fileNameBuffer), fileName);

                LARGE_INTEGER fileSize;
                fileSize.LowPart = findFileData.nFileSizeLow;
                fileSize.HighPart = findFileData.nFileSizeHigh;
                totalSize += fileSize.QuadPart;

                FileTimeToSystemTime(&findFileData.ftCreationTime, &creationTime);
                char dateStr[50];
                snprintf(dateStr, sizeof(dateStr), " - Дата створення: %02d/%02d/%04d %02d:%02d\n",
                    creationTime.wDay, creationTime.wMonth, creationTime.wYear,
                    creationTime.wHour, creationTime.wMinute);

                strncat_s(fileList, sizeof(fileList), fileNameBuffer, _TRUNCATE);
                strncat_s(fileList, sizeof(fileList), dateStr, _TRUNCATE);
            }
        }
    } while (FindNextFileA(hFind, &findFileData) != 0);

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
        std::lock_guard<std::mutex> lock(cacheMutex);

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

        TryWriteToCache();
    }
}

void ReceiveData(SOCKET ClientSocket, char* recvbuf, int recvbuflen) {
    int iResult = recv(ClientSocket, recvbuf, recvbuflen - 1, 0);
    if (iResult > 0) {
        recvbuf[iResult] = '\0';
        printf("Отримано повідомлення: %s\n", recvbuf);
    }
    else if (iResult == 0) {
        printf("З'єднання закрито клієнтом\n");
    }
    else {
        printf("recv завершився з помилкою: %d\n", WSAGetLastError());
    }
}

void SendData(SOCKET ClientSocket, const char* sendbuf) {
    int iResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send завершився з помилкою: %d\n", WSAGetLastError());
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

    while (keepRunning) {
        ReceiveData(ClientSocket, recvbuf, recvbuflen);

        char directoryChoice[MAX_FILENAME_LEN];
        char extension[MAX_EXTENSION_LEN];

        char* spacePos = strchr(recvbuf, '.');
        if (spacePos != NULL) {
            *spacePos = '\0';
            strcpy_s(directoryChoice, recvbuf);
            strcpy_s(extension, spacePos + 1);
        }

        GetDirectoryInfoByExtension(directoryChoice, extension, sendbuf, sizeof(sendbuf));
        {
            std::lock_guard<std::mutex> lock(logMutex);
            if (LogFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
                struct tm localTime;
                localtime_s(&localTime, &currentTime);
                LogFile << std::put_time(&localTime, "%d/%m/%Y %H:%M:%S") << "\n";
                LogFile << "Запит: " << recvbuf << "\nВідповідь:\n" << sendbuf << "\n\n";
            }
        }

        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            cacheData += sendbuf;
            cacheData += "\n";
        }

        SendData(ClientSocket, sendbuf);
    }

    keepRunning = false;
    cacheThread.join();

    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}
