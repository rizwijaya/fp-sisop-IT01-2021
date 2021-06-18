#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include<stdbool.h>

char username[1024], password[1024], database[1024];

void dumpDb(char database[]);
char *trim(char *s);
int login(char id[], char password[]);

int main(int argc, char *argv[]) {
    int is_auth;
    int uid = getuid();
    char credentials[1024];
    if (argc < 2) {
        if(uid == 0) {
            is_auth = login("root","12345");
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
        if (argv[5] != NULL){
            strcpy(database, argv[5]);
            sprintf(database, "%s.backup", database);
        } else {
            printf("Wrong argument");
            return 0;
        }
        is_auth = login(trim(username), trim(password));
    }

    pid_t pid, sid;        // Variabel untuk menyimpan PID
    pid = fork();     // Menyimpan PID dari Child Process

    if (pid < 0) {
    exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    if ((chdir("/home/kali/Desktop/")) < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    while (1) {
        // Tulis program kalian di sini
        int n1 = fork();
        if (n1 == 0){
            // char *argv[] = {"echo", write, ">>", database};
            // execv("/bin/echo", argv);
            dumpDb(database);
        }
        sleep(5);
    }
}

void dumpDb(char database[]){
    //Create File database
    char loc[1024];
    memset(loc, 0, sizeof loc);
    sprintf(loc, "touch %s", database);
    system(loc);

    //Buka File running.log
    FILE* fp;
    char location[1024];
    char *buffer;
    char *dump;

    fp = fopen("../server_db/databases/running.log", "r");
    if (fp == NULL) {
        perror("Failed: ");
    }

    buffer = (char *)malloc(1000 * sizeof(char));
    memset(buffer, 0, 1000 * sizeof(char));
    dump = buffer;
    // -1 to allow room for NULL terminator for really long string
    while (fgets(buffer, 1024 - 1, fp)) {
        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;
        char *token = strtok(buffer, ":");
        int loop = 1;
        while (token != NULL){
            if (loop == 5) {
                sprintf(dump, "%s\n", token);
                //strcpy(dump, buffer);
                dump += strlen(token);
            }
            token = strtok(NULL, ":");
            //printf("%s\n", dump);
            loop++;
        }
    }
    fclose(fp);

    sprintf(location, "/home/kali/Desktop/%s", database);
    fp = fopen(location, "w");
    fprintf(fp, "%s", buffer);
    fclose(fp);
    return;
}

char *trim(char *s) {
    char *ptr;
    if (!s)
        return NULL;   // handle NULL string
    if (!*s)
        return s;      // handle empty string
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

int login(char id[], char password[])
{
    FILE *fp = fopen("../server_db/databases/auth/akun.csv", "r"); //Buka file akun
    int is_auth = 0;
    char buffer[1024];
    while (fgets(buffer, 1024, fp) != NULL && is_auth == 0) {
        char file_id[1024], file_password[1024];
        //Memisahkan id dan password pada file
        char *token = strtok(buffer, ",");
        strcpy(token, trim(token));
        strcpy(file_id, token);
        token = strtok(NULL, "\n");
        strcpy(token, trim(token));
        strcpy(file_password, token);
        //Jika id dan password sesuai, maka return ke 1
        if (strcmp(id, file_id) == 0 && strcmp(password, file_password) == 0) {
            is_auth = 1;
        } else {
            is_auth = 0;
        }
    }
    fclose(fp);
    //printf("login: %d", is_auth);
    return is_auth;
}