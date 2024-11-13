#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <thread>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

std::mutex socketMutex;

void SendDirectoryRequest(SOCKET ConnectSocket, int directoryChoice, const char* fileExtension) {
    char message[DEFAULT_BUFLEN];
    snprintf(message, sizeof(message), "%d%s", directoryChoice, fileExtension);

    std::lock_guard<std::mutex> lock(socketMutex);

    int iResult = send(ConnectSocket, message, (int)strlen(message), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send завершився з помилкою: %d\n", WSAGetLastError());
        return;
    }
    printf("Надіслано запит: директорія %d, розширення %s\n", directoryChoice, fileExtension);

    char recvbuf[DEFAULT_BUFLEN];
    iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN - 1, 0);
    if (iResult > 0) {
        recvbuf[iResult] = '\0';
        printf("Отримано дані:\n%s\n", recvbuf);
    }
    else if (iResult == 0) {
        printf("З'єднання закрито\n");
    }
    else {
        printf("recv завершився з помилкою: %d\n", WSAGetLastError());
    }
}

void ClientProcess(SOCKET ConnectSocket) {
    int directories[] = { 1, 2, 3 };
    const char* extensions[] = { ".txt", ".jpg", ".exe", ".cpp" };

    srand((unsigned int)time(NULL) + GetCurrentThreadId());

    for (int i = 0; i < 20; i++) {
        int directoryChoice = directories[rand() % 3];
        const char* fileExtension = extensions[rand() % 4];

        SendDirectoryRequest(ConnectSocket, directoryChoice, fileExtension);
        Sleep(1000);
    }
}

int __cdecl main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup завершився з помилкою: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char host[] = "127.0.0.1";
    iResult = getaddrinfo(host, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo завершився з помилкою: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket завершився з помилкою: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Не вдалося підключитися до сервера!\n");
        WSACleanup();
        return 1;
    }

    std::thread process1(ClientProcess, ConnectSocket);
    std::thread process2(ClientProcess, ConnectSocket);

    process1.join();
    process2.join();

    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}
