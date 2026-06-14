#include "network_common.h"
#include <cstring>  // Explicitly include for standard compliance
#include <string.h> // Include for older MinGW compatibility

void HandleClient(SocketType clientSocket) {
    char buffer[BUFFER_SIZE];
    std::cout << "[SERVER] Host connected successfully.\n";

    while (true) {
        // Fixed: Using global namespace ::memset to bypass namespace errors
        ::memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        
        if (bytesReceived <= 0) {
            std::cout << "[SERVER] Client connection lost.\n";
            break;
        }

        std::string message(buffer, bytesReceived);

        // Application-Layer Protocol Parsing: Detect File Transfer
        if (message.rfind(FILE_TRANSFER_CMD, 0) == 0) {
            std::string fileName = message.substr(FILE_TRANSFER_CMD.length() + 1);
            std::cout << "[SERVER] Switching to File Mode. Incoming file: " << fileName << "\n";

            std::ofstream outputFile("received_" + fileName, std::ios::binary);
            if (!outputFile.is_open()) {
                std::cerr << "[SERVER] Error: Failed to create local file.\n";
                continue;
            }

            // Stream byte chunks directly into the binary file
            while (true) {
                // Fixed: Using global namespace ::memset here too
                ::memset(buffer, 0, BUFFER_SIZE);
                int fileBytes = recv(clientSocket, buffer, BUFFER_SIZE, 0);
                if (fileBytes <= 0) break;
                
                std::string checkEnd(buffer, fileBytes);
                if (checkEnd == "##EOF##") { // Capture end-of-file signal
                    std::cout << "[SERVER] File transfer complete successfully.\n";
                    break;
                }
                outputFile.write(buffer, fileBytes);
            }
            outputFile.close();
        } else {
            // Standard Text Chat Mode Processing
            std::cout << "\n[Incoming Message]: " << message << "\n> " << std::flush;
        }
    }
    CLOSE_SOCKET(clientSocket);
}

int main() {
    if (!InitializeNetwork()) {
        std::cerr << "Network Initialization Failed.\n";
        return 1;
    }

    SocketType serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET_VAL) {
        std::cerr << "Socket creation error.\n";
        CleanUpNetwork();
        return 1;
    }

    sockaddr_in serverService;
    serverService.sin_family = AF_INET;
    serverService.sin_addr.s_addr = INADDR_ANY;
    serverService.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverService, sizeof(serverService)) == SOCKET_ERR) {
        std::cerr << "Bind failed.\n";
        CLOSE_SOCKET(serverSocket);
        CleanUpNetwork();
        return 1;
    }

    if (listen(serverSocket, 1) == SOCKET_ERR) {
        std::cerr << "Listen failed.\n";
        CLOSE_SOCKET(serverSocket);
        CleanUpNetwork();
        return 1;
    }

    std::cout << "[SERVER] Awaiting incoming cross-platform connection on port " << PORT << "...\n";

    SocketType clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket != INVALID_SOCKET_VAL) {
        HandleClient(clientSocket);
    }

    CLOSE_SOCKET(serverSocket);
    CleanUpNetwork();
    return 0;
}
