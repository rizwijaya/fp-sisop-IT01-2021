#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include<arpa/inet.h>

#define PORT 8080
#define MAX_LENGTH 1000
#define SVR "./databases/"

pthread_t input, received;

void *input_main();

//Error handle nanti
int error(char *err)
{
    perror(err);
    exit(EXIT_FAILURE);
}

struct user
{
    char name[1000];
    char pwd[1000];
    char file[1000];
    char mode[1000];
    char setDb[1000];
    int is_auth;
    int socket;
} user_data;

//Fungsi untuk mengirimkan pesan ke client
void message(char input[])
{
    char buffer[1024];
    sprintf(buffer, "%s", input);
    send(user_data.socket, buffer, 1024, 0);
}

//Fungsi untuk split strings
char * split_string( char *s, const char *delimiter )
{
    static char *p;
    if ( s ) p = s;
    size_t n = strlen( delimiter );
    if ( p && n != 0 ) {
        while ( memcmp( p, delimiter, n ) == 0 ) p += n;
    }
    if ( !p || !*p ) return p = NULL;
    char *t = p;
    if ( n != 0 ) p = strstr( t, delimiter );
    if ( n == 0 || !p ) {
        p = t + strlen( t );
    } else {
        *p = '\0';
        p += n;
    }

    return t;
}
//Fungsi untuk mengecek apakah ada file diserver
int DBExist(char fname[]){
	int found = 0;
	DIR *di;
	struct dirent *dir;
	di = opendir(SVR);
	while ((dir = readdir(di)) != NULL){
		if(strcmp(dir->d_name, fname)==0){
			found=1;
			break;
		}
	}
	closedir(di);
	return found;
}
//Fungsi untuk melakukan login
int login(char id[], char password[])
{
    FILE *fp = fopen("databases/auth/akun.txt", "r"); //Buka file akun
    int is_auth = 0;
    char buffer[1024];
    while (fgets(buffer, 1024, fp) != NULL && is_auth == 0) {
        char file_id[1024], file_password[1024];
        //Memisahkan id dan password pada file
        char *token = strtok(buffer, ":");
        strcpy(file_id, token);
        token = strtok(NULL, "\n");
        strcpy(file_password, token);

        //Jika id dan password sesuai, maka return ke 1
        if (strcmp(id, file_id) == 0 && strcmp(password, file_password) == 0) {
            is_auth = 1;
            strcpy(user_data.name, id);
            strcpy(user_data.pwd, password);
        } else {
            is_auth = 2;
        }
    }
    fclose(fp);
    return is_auth;
}

void catatLog() { //Fungsi Logging mencatat Log

}

int permission(char database[]) { //Fungsi Untuk mengecek Authorisasi
    //bikin file khusus permission ditiap folder database
    //sebelum akses foldernya cek dlu permission difile itu
    //diloop jika nama ada maka langsung bisa akses user jika tidak cetak tidak berhak
    //kemudian apabila user dihapus maka hapus juga
    char loc[1024];
    sprintf(loc, "databases/%s/izin.txt", database);
    FILE *fp = fopen(loc, "r"); //Buka file akun
    int izin = 0;
    char buffer[1024];
    while (fgets(buffer, 1024, fp) != NULL && izin == 0) {
        char file_id[1024], file_password[1024];
        //Melakukan split file authorisasi
        char *token = strtok(buffer, ":");
        strcpy(file_id, token);
        token = strtok(NULL, "\n");
        strcpy(file_password, token);

        //Jika terdapat data user/password maka izinkan
        if (strcmp(user_data.name, file_id) == 0 && strcmp(user_data.pwd, file_password) == 0) {
            izin = 1;
        } else {    //Jika tidak ada, maka tidak berhak
            izin = 0;
        }
    }
    fclose(fp);
    return izin;
}

void createUser(char query[]) {
    int loop = 1;
    char username[1024], password[1024];
    char t1[] = "CREATE USER ";
    char t2[] = " IDENTIFIED BY ";
    //Lakukan split query yang digunakan
    char *p = split_string(query, t1);
    char *q = split_string(p, t2);
    while ( q ) {
        if(loop == 1) {
            sprintf(username, "%s", q);
        } else if(loop == 2) {
            strtok(q, ";");
            sprintf(password, "%s", q);
        }
        q = split_string( NULL, t1 );
        loop++;
    }
    //Input data query kedatabase auth
    FILE *fp = fopen("databases/auth/akun.txt", "a");
    fprintf(fp, "%s:%s\n", username, password);
    fclose(fp);
}

void create(char query[]) { //Fungsi cek CREATE yang digunakan
    int loop = 1;
    char tipe[1024], buffer[1024];
    strcpy(buffer, query);
    char* value = strtok(buffer, " ");
    while ( value != NULL) {
        if (loop == 2) {
            sprintf(tipe, "%s", value);
        }
        value = strtok(NULL, " ");
		loop++;
    }
    if (strcmp(tipe, "USER") == 0) { //Jika tipe adalah USER
        if(permission("auth") == 1) { //mengecek apakah user diizinkan mengakses
            createUser(query);
        } else {
            message("User tidak diizinkan mengakses");
        }
    } else if (strcmp(tipe, "DATABASE") == 0) {
        
    } else if (strcmp(tipe, "TABLE") == 0) {
    
    }
}

void useDb(char query[]) {
    int loop = 1;
    char fname[1024], msg[1024];
    //Lakukan split query yang digunakan
    char *p = strtok(query, " ");
    while ( p ) {
        if(loop == 2) {
            strtok(p, ";");
            sprintf(fname, "%s", p);
        }
        p = strtok(NULL, " ");
        loop++;
    }
    if(permission(fname) == 1) { //mengecek apakah user diizinkan mengakses
        if (DBExist(fname) == 1) { //Jika database ada
            memset(user_data.setDb, 0, 1000);
            strcpy(user_data.setDb, fname);
            sprintf(msg, "Database %s terpilih.\n", user_data.setDb);
            message(msg);
        } else {
            message("Database tidak ada/tidak diizinkan mengakses");
        }
    } else {
        message("Database tidak ada/tidak diizinkan mengakses");
    }
}

void loginsukses()
{
    char msg[1024], buffer[1024], loc[1024];
    //Tampilkan pesan awal
    message("\e[1555557;1H\e[2J\n");
    printf("\nUser %s telah berhasil login.", user_data.name);;
    sprintf(msg, "Selamat datang, %s!\n", user_data.name);
    message(msg);
    sprintf(loc, "\ndb@%s: ", user_data.name);
    message(loc);
    //Lakukan loop hingga exit atau mode berubah
    while (strcmp(buffer, "exit") != 0 || strcmp(user_data.mode, "recvstrings") == 0)
    {
        sprintf(loc, "\ndb@%s: ", user_data.name);
        message(loc);
        read(user_data.socket, buffer, 1024);
        //Memisahkan input dengan filenya
        char cmd_line[MAX_LENGTH];
        strcpy(cmd_line, buffer);
        char *cmd = strtok(cmd_line, " ");

        //Kalau nambah query tinggal edit disini
        if (strcmp(cmd, "CREATE") == 0) { //Jika Query yang diinputkan CREATE
            create(buffer);
        } else if (strcmp(cmd, "USE") == 0) {
            useDb(buffer);
        } else {
            message("Query invalid");
        }
    }
}

void *input_main()
{
    char buffer[1024], username[1024], password[1024];
    while (1) {
        if (user_data.is_auth == 0) { //Login
            read(user_data.socket, buffer, 1024); //Dapatkan data dari pengguna
            for (int i = 0; buffer[i]; i++) { //Merubah input ke lowercase
                buffer[i] = tolower(buffer[i]);
            }
            //Memisahkan username dan password
            char *token = strtok(buffer, ":");
            strcpy(username, token);
            token = strtok(NULL, "\n");
            strcpy(password, token);
            message(username);
            message(password);
            if (strcmp(username, "root") == 0 || strcmp(password, "12345") == 0) {
                //message("root");
                user_data.is_auth = 1; //set auth
            } else {
                user_data.is_auth = login(username, password);
                if (user_data.is_auth == 2) {
                    message("Login Gagal, Silahkan login ulang.");
                    exit(EXIT_FAILURE);
                } else if (user_data.is_auth == 1) {
                    loginsukses();
                }
            }
        } else if (user_data.is_auth == 1) { //Jika berhasil login maka ke process selanjutnya.
            //message("\e[1;1H\e[2J");
            //set user data dan password
            strcpy(user_data.name, username);
            strcpy(user_data.pwd, password);
            loginsukses();
        }
    }
}

int main()
{
    int server_fd, clientsocket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Server berhasil dijalankan\n");
    printf("Menunggu sambungan dari klien.....\n");
    //Jumlah user maksimal 1, jika lebih maka akan mengantri
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    if ((clientsocket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    } else {
        printf("Terhubung dengan klien yang beralamat: %d\n", clientsocket);
        user_data.socket = clientsocket;
    }
    user_data.is_auth = 0; //Menginisialisasi auth dengan 0

    pthread_create(&input, NULL, &input_main, 0);
    while (1)
    { //Agar terus berjalan
        if (pthread_join(input, NULL) == 0)
        {
            pthread_create(&input, NULL, &input_main, 0);
        }
    }
}