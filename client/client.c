#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <zconf.h>

#define MAX_LENGTH 1024
#define PORT 8080
#define CLF "./"

pthread_t input, cetak;

struct user
{
    int is_auth;
    int socket;
    char file[1000];
    char input[1000];
    char mode[1000];
} user_data;

char username[1024], password[1024];

void *user_cetak(void *arg) { //Cetak input user
    if (strcmp(user_data.mode, "recvstrings") == 0) {
        int sock = *(int *)arg;
        char buffer[1024] = {0};
        while (1) {
            memset(buffer, 0, 1024);
            if (recv(sock, buffer, 1024, 0) > 1) {
                char buffer2[1024];
                strcpy(buffer2, buffer);
                char *token = strtok(buffer2, "\n");
                printf("%s", buffer);
            }
        }
    }
}

void *user_input(void *arg)
{
    while (strcmp(user_data.mode, "recvstrings") == 0) {
        char buffer[1024] = {0};
        bzero(buffer, MAX_LENGTH);
        fgets(buffer, MAX_LENGTH, stdin);
        buffer[strcspn(buffer, "\n")] = 0;
        //Mengirimkan input ke server
        send(user_data.socket, buffer, MAX_LENGTH, 0);

        // char cmd_line[MAX_LENGTH];
        // strcpy(cmd_line, buffer);
        // char *cmd = strtok(cmd_line, " "); // Split input

        // for (int i = 0; cmd[i]; i++) { //Merubah input ke lowercase
        //     cmd[i] = tolower(cmd[i]);
        // }
    }
}

int main(int argc, char const *argv[])
{
    struct sockaddr_in address;
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    else
    {
        user_data.socket = sock;
        printf("Terhubung dengan server beralamat %d\n", sock);
    }
    strcpy(user_data.mode, "recvstrings"); //Set mode ke strings
    //Input Login
    int uid = getuid();
    char login[1024];

    if (argc < 2) {
        if(uid == 0) {
            strcpy(login, "root:12345");
            send(user_data.socket, login, MAX_LENGTH, 0);
        } else {
            printf("missing argument\n");
            exit(-1);
        }
    } else {
        if (strcmp(argv[1], "-u") == 0) {
            strcpy(username, argv[2]);
        }
        if (strcmp(argv[3], "-p") == 0) {
            strcpy(password, argv[4]);
        }
        snprintf(login, sizeof login, "%s:%s", username, password);
        send(user_data.socket, login, MAX_LENGTH, 0);
    }

    pthread_create(&cetak, NULL, &user_cetak, (void *)&sock);
    pthread_create(&input, NULL, &user_input, (void *)&sock);
    while(1) {  //Agar terus terhubung dengan server
        if (pthread_join(input, NULL) == 0) {
        pthread_create(&input, NULL, &user_input, (void *)&sock);
        }
    }
    if(strcmp(user_data.mode, "recvstrings") == 0) {
        pthread_join(cetak, NULL);
    } else {
        pthread_exit(&cetak);
    }
}