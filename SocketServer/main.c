#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

struct AcceptedSocket {
    int acceptedSocketFD;
    struct sockaddr_in address;
    bool acceptedSuccessfully;
    char username[50];
};

void startAcceptingIncomingConnections(int serverSocketFD);
void *receiveAndPrintIncomingData(void *clientSocket);
void sendReceivedMessageToTheOtherClients(char *buffer, struct AcceptedSocket *sender);
void removeClient(struct AcceptedSocket *client);

struct AcceptedSocket acceptedSockets[MAX_CLIENTS];
int acceptedSocketsCount = 0;
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

void startAcceptingIncomingConnections(int serverSocketFD) {
    while (true) {
        struct AcceptedSocket clientSocket;
        socklen_t addrSize = sizeof(clientSocket.address);

        clientSocket.acceptedSocketFD = accept(serverSocketFD, (struct sockaddr *)&clientSocket.address, &addrSize);
        if (clientSocket.acceptedSocketFD < 0) {
            perror("Failed to accept connection");
            continue;
        }

        clientSocket.acceptedSuccessfully = true;

        pthread_mutex_lock(&clientsMutex);
        if (acceptedSocketsCount < MAX_CLIENTS) {
            acceptedSockets[acceptedSocketsCount++] = clientSocket;
        } else {
            printf("Max clients reached. Connection refused.\n");
            close(clientSocket.acceptedSocketFD);
            pthread_mutex_unlock(&clientsMutex);
            continue;
        }
        pthread_mutex_unlock(&clientsMutex);

        pthread_t id;
        struct AcceptedSocket *newClient = malloc(sizeof(struct AcceptedSocket));
        if (!newClient) {
            perror("Memory allocation failed");
            close(clientSocket.acceptedSocketFD);
            continue;
        }
        *newClient = clientSocket;
        pthread_create(&id, NULL, receiveAndPrintIncomingData, (void *)newClient);
        pthread_detach(id);
    }
}

void *receiveAndPrintIncomingData(void *clientSocket) {
    struct AcceptedSocket *client = (struct AcceptedSocket *)clientSocket;
    int socketFD = client->acceptedSocketFD;
    char buffer[BUFFER_SIZE];

    ssize_t usernameReceived = recv(socketFD, client->username, sizeof(client->username) - 1, 0);
    if (usernameReceived <= 0) {
        printf("Failed to receive username.\n");
        close(socketFD);
        free(client);
        return NULL;
    }
    client->username[usernameReceived] = '\0';
    printf("%s has joined the chat.\n", client->username);

    char joinMessage[BUFFER_SIZE];
    snprintf(joinMessage, sizeof(joinMessage), "%s has joined the chat.", client->username);
    sendReceivedMessageToTheOtherClients(joinMessage, client);

    while (true) {
        ssize_t amountReceived = recv(socketFD, buffer, sizeof(buffer) - 1, 0);
        if (amountReceived > 0) {
            buffer[amountReceived] = '\0';
            printf("%s\n",  buffer); // this is on the server side

            // char formattedMessage[BUFFER_SIZE + 50];
            // snprintf(buffer, sizeof(buffer), "%s: %.900s", client->username, buffer);
            sendReceivedMessageToTheOtherClients(buffer, client);
        } else {
            printf("%s disconnected.\n", client->username);
            break;
        }
    }

    close(socketFD);
    removeClient(client);

    char leaveMessage[BUFFER_SIZE];
    snprintf(leaveMessage, sizeof(leaveMessage), "%s has left the chat.", client->username);
    sendReceivedMessageToTheOtherClients(leaveMessage, client);

    free(client);
    return NULL;
}

void sendReceivedMessageToTheOtherClients(char *buffer, struct AcceptedSocket *sender) {
    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < acceptedSocketsCount; i++) {
        if (acceptedSockets[i].acceptedSocketFD != sender->acceptedSocketFD) {
            send(acceptedSockets[i].acceptedSocketFD, buffer, strlen(buffer), 0);
        }
    }
    pthread_mutex_unlock(&clientsMutex);
}

void removeClient(struct AcceptedSocket *client) {
    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < acceptedSocketsCount; i++) {
        if (acceptedSockets[i].acceptedSocketFD == client->acceptedSocketFD) {
            for (int j = i; j < acceptedSocketsCount - 1; j++) {
                acceptedSockets[j] = acceptedSockets[j + 1];
            }
            acceptedSocketsCount--;
            break;
        }
    }
    pthread_mutex_unlock(&clientsMutex);
}

int main() {
    int serverSocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocketFD < 0) {
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    setsockopt(serverSocketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serverAddress = {0};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(2000);

    if (bind(serverSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Binding failed");
        return -1;
    }
    printf("Socket bound successfully\n");

    if (listen(serverSocketFD, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        return -1;
    }

    printf("Server is listening on port 2000...\n");
    startAcceptingIncomingConnections(serverSocketFD);

    shutdown(serverSocketFD, SHUT_RDWR);
    close(serverSocketFD);
    return 0;
}
