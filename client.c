#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char username[50];

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        printf("Soket oluşturulamadı\n");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bağlanılamadı\n");
        return -1;
    }

    printf("Sunucuya bağlanıldı!\n");

    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        printf("Bağlantı hatası\n");
        close(client_socket);
        return -1;
    }
    buffer[bytes_received] = '\0';
    printf("%s", buffer);

    fgets(username, sizeof(username), stdin);
    send(client_socket, username, strlen(username), 0);

    while (1) {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Sunucu ile bağlantı koptu\n");
            break;
        }

        buffer[bytes_received] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, "Quiz Tamamlandı!") != NULL) {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("%s", buffer);
            }
            break;
        }

        if (strstr(buffer, "Cevabın") != NULL) {
            char answer[10];
            fgets(answer, sizeof(answer), stdin);
            send(client_socket, answer, strlen(answer), 0);
        }
    }

    printf("\nÇıkmak için Enter tuşuna basınız");
    getchar();
    close(client_socket);
    return 0;
}