#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define MAX_FILENAME_LEN 260

// Функція для отримання інформації про файли з відповідним розширенням
void GetDirectoryInfoByExtension(const char* extension, char* buffer, size_t bufferSize) {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind;
    const wchar_t* dirPath = L"E:\\Операційні системи\\Лабораторні\\Лаб.9\\durektoriya1\\*";

    hFind = FindFirstFileW(dirPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        snprintf(buffer, bufferSize, "Не вдалося знайти директорію.");
        return;
    }

    size_t totalSize = 0;
    char fileList[DEFAULT_BUFLEN] = "";
    SYSTEMTIME creationTime;

    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // Перетворення імені файлу з WCHAR в звичайний рядок
            char fileName[MAX_FILENAME_LEN];
            size_t convertedChars = 0;
            wcstombs_s(&convertedChars, fileName, MAX_FILENAME_LEN, findFileData.cFileName, _TRUNCATE);

            // Перевіряємо, чи файл відповідає заданому розширенню
            if (strstr(fileName, extension) != NULL) {
                // Додаємо ім'я файлу до списку
                strncat_s(fileList, DEFAULT_BUFLEN, fileName, _TRUNCATE);
                strncat_s(fileList, DEFAULT_BUFLEN, "\n", _TRUNCATE);

                // Отримуємо розмір файлу
                LARGE_INTEGER fileSize;
                fileSize.LowPart = findFileData.nFileSizeLow;
                fileSize.HighPart = findFileData.nFileSizeHigh;
                totalSize += fileSize.QuadPart;

                // Отримуємо дату створення файлу
                FileTimeToSystemTime(&findFileData.ftCreationTime, &creationTime);
                char dateStr[50];
                snprintf(dateStr, sizeof(dateStr),
                    " - Дата створення: %02d/%02d/%04d %02d:%02d\n",
                    creationTime.wDay, creationTime.wMonth, creationTime.wYear,
                    creationTime.wHour, creationTime.wMinute);
                strncat_s(fileList, DEFAULT_BUFLEN, dateStr, _TRUNCATE);
            }
        }
    } while (FindNextFileW(hFind, &findFileData) != 0);

    FindClose(hFind);

    // Формуємо підсумкове повідомлення
    snprintf(buffer, bufferSize,
        "Сумарний розмір файлів: %llu байт\nСписок файлів:\n%s",
        totalSize, fileList);
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

    // Ініціалізація Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Розв'язування адреси і порту
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Сервер очікує підключення...\n");

    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    closesocket(ListenSocket);

    // Приймаємо розширення файлів від клієнта
    iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
    if (iResult > 0) {
        recvbuf[iResult] = '\0';  // Закінчуємо рядок
        printf("Received file extension: %s\n", recvbuf);

        // Отримуємо інформацію про файли з відповідним розширенням
        GetDirectoryInfoByExtension(recvbuf, sendbuf, sizeof(sendbuf));

        // Відправляємо інформацію клієнту
        iResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
        }
    }
    else {
        printf("recv failed with error: %d\n", WSAGetLastError());
    }

    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}
