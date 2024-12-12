#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024
#define NUM_QUESTIONS 5
#define MAX_LEADERBOARD 3
#define MIN(a,b) ((a) < (b) ? (a) : (b))

typedef struct {
    char question[256];
    char options[4][64];
    int correct_answer;
} Question;

typedef struct {
    int socket;
    char name[50];
    int score;
    int current_question;
} Client;

typedef struct {
    char name[50];
    int score;
} LeaderboardEntry;

Question questions[NUM_QUESTIONS] = {
    {"Aşağıdaki dillerden hangisi bir programlama dili değildir?", 
     {"Python", "Java", "HTML", "C++"}, 2},
    
    {"Aşağıdakilerden hangisi bir programlama dilidir?", 
     {"HTML", "C", "CSS", "SQL"}, 1},
    
    {"Bilgisayarın geçici hafızası nedir?", 
     {"Sabit disk", "RAM", "ROM", "Flash bellek"}, 1},
    
    {"1 bayt kaç bit içerir?", 
     {"4", "8", "16", "32"}, 1},
    
    {"Bir programın hatalarını bulup düzeltme işlemine ne denir?", 
     {"Kodlama", "Derleme", "Debugging", "Test etme"}, 2},
};

Client clients[MAX_CLIENTS];
LeaderboardEntry leaderboard[MAX_LEADERBOARD];
int client_count = 0;
int leaderboard_count = 0;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t leaderboard_lock = PTHREAD_MUTEX_INITIALIZER;

void update_leaderboard(const char* name, int score) {
    pthread_mutex_lock(&leaderboard_lock);
    
    int inserted = 0;
    for (int i = 0; i < leaderboard_count && i < MAX_LEADERBOARD; i++) {
        if (score > leaderboard[i].score) {
            for (int j = MIN(leaderboard_count, MAX_LEADERBOARD - 1); j > i; j--) {
                leaderboard[j] = leaderboard[j-1];
            }
            strncpy(leaderboard[i].name, name, 49);
            leaderboard[i].name[49] = '\0';
            leaderboard[i].score = score;
            inserted = 1;
            if (leaderboard_count < MAX_LEADERBOARD) leaderboard_count++;
            break;
        }
    }

    if (!inserted && leaderboard_count < MAX_LEADERBOARD) {
        strncpy(leaderboard[leaderboard_count].name, name, 49);
        leaderboard[leaderboard_count].name[49] = '\0';
        leaderboard[leaderboard_count].score = score;
        leaderboard_count++;
    }

    pthread_mutex_unlock(&leaderboard_lock);
}

void send_leaderboard(int client_socket) {
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&leaderboard_lock);
    
    snprintf(buffer, BUFFER_SIZE, "\n=== Lider Tablosu %d ===\n", MAX_LEADERBOARD);
    send(client_socket, buffer, strlen(buffer), 0);
    
    for (int i = 0; i < leaderboard_count; i++) {
        snprintf(buffer, BUFFER_SIZE, "%d. %s: %d puan\n", 
                i + 1, leaderboard[i].name, leaderboard[i].score);
        send(client_socket, buffer, strlen(buffer), 0);
    }
    
    if (leaderboard_count == 0) {
        snprintf(buffer, BUFFER_SIZE, "Henüz lider yok!\n");
        send(client_socket, buffer, strlen(buffer), 0);
    }
    
    send(client_socket, "\n", 1, 0);
    pthread_mutex_unlock(&leaderboard_lock);
}

void send_question(int client_socket, int question_index) {
    char buffer[BUFFER_SIZE];
    Question q = questions[question_index];
    
    snprintf(buffer, BUFFER_SIZE, "\nSoru %d/%d: %s\n1) %s\n2) %s\n3) %s\n4) %s\nCevabınız (1-4): ",
             question_index + 1, NUM_QUESTIONS, q.question, 
             q.options[0], q.options[1], q.options[2], q.options[3]);
    
    send(client_socket, buffer, strlen(buffer), 0);
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes_received;
    int current_client_index = -1;

    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_socket) {
            current_client_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&client_lock);

    if (current_client_index == -1) {
        close(client_socket);
        return NULL;
    }

    send_leaderboard(client_socket);

    send_question(client_socket, clients[current_client_index].current_question);

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0';
        int answer = atoi(buffer);
        
        if (answer < 1 || answer > 4) {
            send(client_socket, "1 ile 4 arasında bir sayı giriniz.\n", 38, 0);
            continue;
        }

        pthread_mutex_lock(&client_lock);
        Question current_q = questions[clients[current_client_index].current_question];
        
        if (answer - 1 == current_q.correct_answer) {
            clients[current_client_index].score++;
            char correct_msg[BUFFER_SIZE];
            snprintf(correct_msg, BUFFER_SIZE, "Doğru! Puanınız: %d\n", 
                    clients[current_client_index].score);
            send(client_socket, correct_msg, strlen(correct_msg), 0);
        } else {
            char wrong_msg[BUFFER_SIZE];
            snprintf(wrong_msg, BUFFER_SIZE, "Yanlış! Doğru cevap: %s\n", 
                    current_q.options[current_q.correct_answer]);
            send(client_socket, wrong_msg, strlen(wrong_msg), 0);
        }

        clients[current_client_index].current_question++;
        
        if (clients[current_client_index].current_question >= NUM_QUESTIONS) {
            char final_msg[BUFFER_SIZE];
            snprintf(final_msg, BUFFER_SIZE, "\nQuiz Bitti! Toplam Puanınız: %d/%d\n", 
                    clients[current_client_index].score, NUM_QUESTIONS);
            send(client_socket, final_msg, strlen(final_msg), 0);
            
            update_leaderboard(clients[current_client_index].name, clients[current_client_index].score);
            
            send_leaderboard(client_socket);
            
            pthread_mutex_unlock(&client_lock);
            break;
        }

        send_question(client_socket, clients[current_client_index].current_question);
        pthread_mutex_unlock(&client_lock);
    }

    pthread_mutex_lock(&client_lock);
    for (int i = current_client_index; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
    }
    client_count--;
    pthread_mutex_unlock(&client_lock);

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pthread_t tid;

    srand(time(NULL));

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, MAX_CLIENTS);

    printf("Sunucu çalışmaya başladı. Port: %d\n", PORT);

    while (1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

        pthread_mutex_lock(&client_lock);
        if (client_count >= MAX_CLIENTS) {
            send(client_socket, "Sunucu Dolu. Lütfen daha sonra tekrar deneyiniz.\n", 32, 0);
            close(client_socket);
            pthread_mutex_unlock(&client_lock);
            continue;
        }

        char welcome_msg[] = "Hoşgeldiniz! Lütfen bir kullanıcı adı giriniz: ";
        send(client_socket, welcome_msg, strlen(welcome_msg), 0);
        
        char username[50];
        int bytes_received = recv(client_socket, username, sizeof(username) - 1, 0);
        if (bytes_received > 0) {
            username[bytes_received] = '\0';
            username[strcspn(username, "\n")] = '\0';
        } else {
            close(client_socket);
            pthread_mutex_unlock(&client_lock);
            continue;
        }

        clients[client_count].socket = client_socket;
        clients[client_count].score = 0;
        clients[client_count].current_question = 0;
        strncpy(clients[client_count].name, username, sizeof(clients[client_count].name) - 1);
        clients[client_count].name[sizeof(clients[client_count].name) - 1] = '\0';
        
        char welcome[BUFFER_SIZE];
        snprintf(welcome, BUFFER_SIZE, "Hoş Geldin %s! quiz için hazır ol \n\n", 
                clients[client_count].name);
        send(client_socket, welcome, strlen(welcome), 0);

        client_count++;
        pthread_mutex_unlock(&client_lock);

        int *client_sock = malloc(sizeof(int));
        *client_sock = client_socket;
        pthread_create(&tid, NULL, handle_client, client_sock);
    }

    close(server_socket);
    return 0;
}