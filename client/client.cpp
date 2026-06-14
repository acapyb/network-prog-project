#include "network_common.h"

void SendFile(SocketType serverSocket, const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[CLIENT] Error: Cannot open file target: " << filePath << "\n";
        return;
    }

    // Isolate file name from path string
    std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    std::string initCmd = FILE_TRANSFER_CMD + " " + fileName;
    
    // Alert server regarding file streaming transition
    send(serverSocket, initCmd.c_str(), initCmd.length(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); // Sync buffer delay

    char buffer[BUFFER_SIZE];
    std::cout << "[CLIENT] Streaming data byte blocks...\n";
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        send(serverSocket, buffer, file.gcount(), 0);
    }
    file.close();

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    std::string eofSignal = "##EOF##";
    send(serverSocket, eofSignal.c_str(), eofSignal.length(), 0); // Send protocol termination sequence
    std::cout << "[CLIENT] File data fully streamed.\n";
}

void ReceiveMessages(SocketType serverSocket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        std::memset(buffer, 0, BUFFER_SIZE);
        int bytesRecv = recv(serverSocket, buffer, BUFFER_SIZE, 0);
        if (bytesRecv <= 0) {
            std::cout << "[CLIENT] Remote server closed connection channel.\n";
            break;
        }
        std::cout << "\n[Server Response]: " << std::string(buffer, bytesRecv) << "\n> " << std::flush;
    }
}

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
    inet_pton(AF_INET, ipAddress.c_str(), &clientService.sin_addr);

    if (connect(clientSocket, (sockaddr*)&clientService, sizeof(clientService)) == SOCKET_ERR) {
        std::cerr << "Inbound Connection Failure.\n";
        CLOSE_SOCKET(clientSocket);
        CleanUpNetwork();
        return 1;
    }

    std::cout << "Connection Established! Use '/sendfile <file_path>' or type text directly.\n";

    // Asynchronous thread execution to handle incoming message loops
    std::thread rxThread(ReceiveMessages, clientSocket);
    rxThread.detach();

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
