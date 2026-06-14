#define _WIN32_WINNT 0x0601

#include "network_common.h"
#include <chrono>
#include <cstring>
#include <string.h>

// Explicitly include Windows native system definitions for threading fallback
#ifdef _WIN32
    #include <windows.h>
#else
    #include <thread>
#endif

void SendFile(SocketType serverSocket, const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[CLIENT] Error: Cannot open file target: " << filePath << "\n";
        return;
    }

    std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    std::string initCmd = FILE_TRANSFER_CMD + " " + fileName;
    
    send(serverSocket, initCmd.c_str(), initCmd.length(), 0);
    
    // Fallback synchronization pause using Win32 Sleep or standard POSIX thread sleep
#ifdef _WIN32
    Sleep(250); // Windows native millisecond delay execution
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
#endif

    char buffer[BUFFER_SIZE];
    std::cout << "[CLIENT] Streaming data byte blocks...\n";
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        send(serverSocket, buffer, file.gcount(), 0);
    }
    file.close();

#ifdef _WIN32
    Sleep(250);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
#endif

    std::string eofSignal = "##EOF##";
    send(serverSocket, eofSignal.c_str(), eofSignal.length(), 0);
    std::cout << "[CLIENT] File data fully streamed.\n";
}

void ReceiveMessages(SocketType serverSocket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        ::memset(buffer, 0, BUFFER_SIZE);
        int bytesRecv = recv(serverSocket, buffer, BUFFER_SIZE, 0);
        if (bytesRecv <= 0) {
            std::cout << "[CLIENT] Remote server closed connection channel.\n";
            break;
        }
        std::cout << "\n[Server Response]: " << std::string(buffer, bytesRecv) << "\n> " << std::flush;
    }
}

// C-style wrapper function matching Windows CreateThread execution signatures
#ifdef _WIN32
DWORD WINAPI WindowsThreadWrapper(LPVOID lpParam) {
    SocketType* sockPtr = (SocketType*)lpParam;
    ReceiveMessages(*sockPtr);
    return 0;
}
#endif

int main() {
    if (!InitializeNetwork()) return 1;

    SocketType clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET_VAL) {
        CleanUpNetwork();
        return 1;
    }

    std::string ipAddress;
    std::cout << "Enter Destination Target Host IP Address: ";
    std::getline(std::cin, ipAddress);

    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(PORT);
    
#ifdef _WIN32
    clientService.sin_addr.s_addr = inet_addr(ipAddress.c_str());
#else
    inet_pton(AF_INET, ipAddress.c_str(), &clientService.sin_addr);
#endif

    if (connect(clientSocket, (sockaddr*)&clientService, sizeof(clientService)) == SOCKET_ERR) {
        std::cerr << "Inbound Connection Failure.\n";
        CLOSE_SOCKET(clientSocket);
        CleanUpNetwork();
        return 1;
    }

    std::cout << "Connection Established! Use '/sendfile <file_path>' or type text directly.\n";

    // Launch background execution using native system handling interfaces
#ifdef _WIN32
    HANDLE threadHandle = CreateThread(NULL, 0, WindowsThreadWrapper, &clientSocket, 0, NULL);
    if (threadHandle != NULL) {
        CloseHandle(threadHandle); // Detach immediately to let it run freely
    }
#else
    std::thread rxThread(ReceiveMessages, clientSocket);
    rxThread.detach();
#endif

    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input == "exit") break;

        if (input.rfind(FILE_TRANSFER_CMD, 0) == 0) {
            if (input.length() > FILE_TRANSFER_CMD.length() + 1) {
                std::string filePath = input.substr(FILE_TRANSFER_CMD.length() + 1);
                SendFile(clientSocket, filePath);
            } else {
                std::cout << "Format rule error. Use: /sendfile <file_path>\n";
            }
        } else {
            send(clientSocket, input.c_str(), input.length(), 0);
        }
    }

    CLOSE_SOCKET(clientSocket);
    CleanUpNetwork();
    return 0;
}
