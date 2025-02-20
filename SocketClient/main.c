#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <stdint.h>
#include <arpa/inet.h>

struct ClientInfo {
    int socketFD;
    char user_name[50];
};

// Function prototypes
void *listenForMessages(void *clientInfo);
void readConsoleEntriesAndSendToServer(struct ClientInfo *clientInfo);

int main() {
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in serverAddress = {0};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(2000);

    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == 0) {
        printf("Connected to the server successfully.\n");
    } else {
        perror("Connection failed");
        close(socketFD);
        return -1;
    }

    // Ask user for their name
    struct ClientInfo clientInfo;
    clientInfo.socketFD = socketFD;
    printf("Enter your name: ");
    scanf("%49s", clientInfo.user_name);
    getchar();  // Consume the newline character left in stdin

    // Send username to the server for registration
    send(socketFD, clientInfo.user_name, strlen(clientInfo.user_name), 0);

    // Create a thread to listen for incoming messages
    pthread_t listenThread;
    pthread_create(&listenThread, NULL, listenForMessages, (void*)&clientInfo);

    // Read user input and send messages
    readConsoleEntriesAndSendToServer(&clientInfo);

    // Close socket before exiting
    close(socketFD);
    return 0;
}

// Function to listen for messages from the server
void *listenForMessages(void *arg) {
    struct ClientInfo *clientInfo = (struct ClientInfo *)arg;
    int socketFD = clientInfo->socketFD;
    char buffer[1024];

    while (true) {
        ssize_t amountReceived = recv(socketFD, buffer, sizeof(buffer) - 1, 0); // this buffer doesn't have client name in it.
        if (amountReceived > 0) {
            buffer[amountReceived] = '\0';

            // char sender[50], message[950];
            // if (sscanf(buffer, "%49[^:]: %[^\n]", sender, message) == 2) {
            //     if (strcmp(clientInfo->user_name, sender) == 0) {
            //         printf("\nYou: %s\n", message);
            //     } else {
            //         printf("\n%s: %s\n", sender, message);
            //     }
            // } else {
            //     printf("\n%s\n", buffer);
            // }
            printf("\n%s\n", buffer);
            printf("You: ");
            fflush(stdout);
        } else if (amountReceived == 0) {
            printf("\nServer disconnected.\n");
            break;
        } else {
            perror("recv failed");
            break;
        }
    }

    close(socketFD);
    return NULL;
}


// Function to read user input and send messages
void readConsoleEntriesAndSendToServer(struct ClientInfo *clientInfo) {
    char line[1024];
    printf("Type your message (type 'exit' to quit)...\n");

    while (true) {
        printf("You: ");
        fflush(stdout);
        fgets(line, sizeof(line), stdin);
        line[strcspn(line, "\n")] = 0; // Remove newline character

        if (strcmp(line, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        // Ensure the message fits within the buffer
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s: %.900s", clientInfo->user_name, line);
        send(clientInfo->socketFD, buffer, strlen(buffer), 0);
    }
}

