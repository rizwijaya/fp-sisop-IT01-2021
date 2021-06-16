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
#include <assert.h>

#define PORT 8080
#define MAX_LENGTH 1000
#define SVR "./databases/"
#define DELIM ",\n"

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
    sprintf(buffer, "%s\n", input);
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
//Fungsi untuk mengecek apakah ada table di database
int TableExist(char table[]){
    char folder[1024];
	int found = 0;
	DIR *di;
	struct dirent *dir;
    sprintf(folder, "databases/%s/", user_data.setDb);
	di = opendir(folder);
	while ((dir = readdir(di)) != NULL){
		if(strcmp(dir->d_name, table)==0){
			found=1;
			break;
		}
	}
	closedir(di);
	return found;
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

void catatLog(char argument[]) { //Fungsi Logging mencatat Log
    time_t rawtime;
    struct tm * timeinfo;
    char time_now[26];
    //dapatkan waktu sekarang
    time( & rawtime);
    timeinfo = localtime( & rawtime);
    strftime(time_now, 26, "%F %H:%M:%S", timeinfo); 
    FILE *fp;
    fp = fopen("databases/running.log", "a+");
    fprintf(fp, "\n%s:%s:%s", time_now, user_data.name, argument);
    fclose(fp);
}

int permission(char database[]) { //Fungsi Untuk mengecek Authorisasi
    //bikin file khusus permission ditiap folder database
    //sebelum akses foldernya cek dlu permission difile itu
    //diloop jika nama ada maka langsung bisa akses user jika tidak cetak tidak berhak
    //kemudian apabila user dihapus maka hapus juga
    char loc[1024];
    sprintf(loc, "databases/%s/izin.csv", database);
    FILE *fp = fopen(loc, "r"); //Buka file akun
    int izin = 0;
    char buffer[1024];
    while (fgets(buffer, 1024, fp) != NULL && izin == 0) {
        char file_id[1024];
        //Melakukan split file authorisasi
        char *token = strtok(buffer, "\n");
        strcpy(file_id, token);
        //token = strtok(NULL, "\n");

        //Jika terdapat data user/password maka izinkan
        if (strcmp(user_data.name, file_id) == 0) {
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
    FILE *fp = fopen("databases/auth/akun.csv", "a");
    fprintf(fp, "%s,%s\n", username, password);
    fclose(fp);
}

void remove_all_chars(char* str, char c);

void createTable(const char* query){
    // if setDb exist
    if (strcmp(user_data.setDb, "kosong") != 0){

        int loop = 1;
        char tablePath[1024], tableName[1024], content[1024], tableData[1024], tableDataType[1024], buffer[1024];
        memset(tableData, 0, sizeof tableData);
        memset(tableDataType, 0, sizeof tableDataType);
        
        // ambil isi tabel
        strcpy(buffer, query);
        remove_all_chars(buffer, ';'); 
        remove_all_chars(buffer, ','); 
        char* value = strtok(buffer, " ");
        while ( value != NULL) {
            if (loop == 3) {
                sprintf(tableName, "%s", value);
            }
            if (loop > 3 && !(loop%2)) {
                if (loop != 4) strcat(tableData, ",");
                strcat(tableData, value);
            }
            if (loop > 4 && (loop%2)) {
                if (loop != 5) strcat(tableDataType, ",");
                strcat(tableDataType, value);
            }
            value = strtok(NULL, " ");
            loop++;
        }
        remove_all_chars(tableData, '(');
        remove_all_chars(tableDataType, ')');

        // set the file path and content
        sprintf(tablePath, "touch databases/%s/%s.csv", user_data.setDb, tableName);
        system(tablePath);
        sprintf(tablePath, "databases/%s/%s.csv", user_data.setDb, tableName);
        sprintf(content, "%s\n%s", tableData, tableDataType);
        message(content);

        // write to file
        FILE *fp = fopen(tablePath, "w+");
        fprintf(fp, "%s", content);
        fclose(fp);
        
        // success message
        message("Tabel berhasil dibuat");
    } else {
        message("Anda belum memilih database");
    }
}

// TL; DR, untuk replace substring dalam string
char *strrep(const char *s1, const char *s2, const char *s3);

void insert(const char* query){
    if (strcmp(user_data.setDb, "kosong") != 0){
        int loop = 1;
        char buffer[1024], tableName[1024], tableRow[1024], tablePath[1024], content[1024];
        
        strcpy(buffer, query);
        // remove char penganggu
        remove_all_chars(buffer, ';');
        
        char* value = strtok(buffer, " ");
        while ( value != NULL){
            if (loop == 2 && (strcmp(value, "INTO") != 0)) message("Query invalid");
            if (loop == 3) strcpy(tableName, value);
            value = strtok(NULL, " ");
            loop++;
        }

        // get table value
        memset(buffer, 0, sizeof buffer);
        strcpy(buffer, query);
        
        // dapatkan value dari okurensi awal kurung buka
        char* tableValue = strchr(buffer, '(');
        
        // hilangkan kurungnya biar gampang
        remove_all_chars(tableValue, '(');
        remove_all_chars(tableValue, ')');
        remove_all_chars(tableValue, '\'');
        
        // hilangkan ", "
        char *str = strrep(tableValue, ", ", ",");
        strcpy(tableValue, str);
        free(str);

        // output
        strcat(tableName, ".csv");
        if (TableExist(tableName) == 1){
            // setting path
            message(tableValue);
            sprintf(tablePath, "databases/%s/%s", user_data.setDb, tableName);
            sprintf(content, "\n%s", tableValue);

            // append to file
            FILE *fp = fopen(tablePath, "a");
            fprintf(fp, "%s", content);
            fclose(fp);

            message("Sukses menambahkan data.");
        } else {
            sprintf(buffer, "Tabel %s tidak ada di database %s", tableName, user_data.setDb);
            message(buffer);
        }
    } else{
        message("Anda belum memilih database");
    }
}

void createDb(char query[]) {
    int loop = 1;
    char database[1024], msg[1024], folder[1024], file[1024], isifile[1024];
    //Lakukan split query yang digunakan
    char *p = strtok(query, " ");
    while ( p ) {
        if(loop == 3) {
            strtok(p, ";");
            sprintf(database, "%s", p);
        }
        p = strtok(NULL, " ");
        loop++;
    }
    //Buat folder database
    sprintf(folder, "mkdir databases/%s", database);
    system(folder);
    //Buat Table/File izin
    sprintf(file, "touch databases/%s/izin.csv", database);
    system(file);
    //Isi Table/File izin dengan heading izin dan kemudian nama user
    sprintf(isifile, "databases/%s/izin.csv", database);
    FILE *fp = fopen(isifile, "a");
    fprintf(fp, "izin\n%s", user_data.name);
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
        if(permission(tipe) == 1) { //mengecek apakah user diizinkan mengakses
            createUser(query);
        } else {
            message("User tidak diizinkan mengakses");
        }
    } else if (strcmp(tipe, "DATABASE") == 0) {
        createDb(query);
    } else if (strcmp(tipe, "TABLE") == 0) { //Belum Selesai
        createTable(query);
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
    
    if(DBExist( fname) == 1) { //mengecek apakah user diizinkan mengakses
        if (permission(fname) == 1) { //Jika database ada
            memset(user_data.setDb, 0, 1000);
            strcpy(user_data.setDb, fname);
            sprintf(msg, "\nDatabase %s terpilih.", user_data.setDb);
            message(msg);
        } else {
            message("\nDatabase tidak ada/tidak diizinkan mengakses");
        }
    } else {
        message("\nDatabase tidak ada/tidak diizinkan mengakses");
    }
}

void ChangePermission(char query[]) {
    int loop = 1;
    char database[1024], nama[1024], folder[1024];
    char t1[] = "GRANT PERMISSION ";
    char t2[] = " INTO ";
    //Lakukan split query yang digunakan
    char *p = split_string(query, t1);
    char *q = split_string(p, t2);
    while ( q ) {
        if(loop == 1) {
            sprintf(database, "%s", q);
        } else if(loop == 2) {
            strtok(q, ";");
            sprintf(nama, "%s", q);
        }
        q = split_string( NULL, t1 );
        loop++;
    }
    sprintf(folder, "databases/%s/izin.csv", database);
    //Input data query kedatabase auth
    FILE *fp = fopen(folder, "a");
    fprintf(fp, "\n%s", nama);
    fclose(fp);
}

void dropDb(char query[]) {
    int loop = 1;
    char database[1024], msg[1024], folder[1024];
    //Lakukan split query yang digunakan
    char *p = strtok(query, " ");
    while ( p ) {
        if(loop == 3) {
            strtok(p, ";");
            sprintf(database, "%s", p);
        }
        p = strtok(NULL, " ");
        loop++;
    }
    if (DBExist(database) == 1) { //Jika database ada
        if(permission(database) == 1) { //mengecek apakah user diizinkan mengakses
            sprintf(folder, "rm -r databases/%s", database);
            system(folder);
            message("\nDatabase berhasil dihapus.");
        } else {
            message("\nUser tidak diizinkan untuk menghapus."); 
        }    
    } else {
        message("\nDatabase berhasil dihapus.");
    }
}

void dropTable(char query[]) { //Fungsi untuk menghapus table
    int loop = 1;
    char table[1024], msg[1024], folder[1024];
    //Lakukan split query yang digunakan
    char *p = strtok(query, " ");
    while ( p ) {
        if(loop == 3) {
            strtok(p, ";");
            sprintf(table, "%s.csv", p);
        }
        p = strtok(NULL, " ");
        loop++;
    }
    // puts(table);
    // printf("%s", user_data.setDb);
    if(strcmp(user_data.setDb, "kosong") != 0) { //Jika db sudah di use
        if (TableExist(table) == 1) { //Jika table ada
            sprintf(folder, "rm -r databases/%s/%s", user_data.setDb ,table);
            system(folder);
            message("\nTable berhasil dihapus.");  
        } else {
            message("\nTable berhasil dihapus.");
        }
    } else {
        message("\nSilahkan set database terlebih dahulu.");
    }
}

char** cutArray(char* a_str, const char a_delim) { //Fungsi untuk melakukan cut pada sebuah array
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    while (*tmp) {
        if (a_delim == *tmp) {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    count += last_comma < (a_str + strlen(a_str) - 1);
    count++;

    result = malloc(sizeof(char*) * count);

    if (result) {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token) {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

int GetColumn(char table[], char word[], int* in, int* fou, int* row) { //Fungsi untuk get Index column dengan nama
    char data[1024], folder[1024];
    sprintf(folder, "databases/%s/%s", user_data.setDb, table);
	FILE *fp = fopen(folder, "r");
	if(fgets(data, sizeof(data), fp))
	fclose(fp);

    char** tokens;
    char cv[10][10];
    int akhir, i;

    tokens = cutArray(data, ',');
    
    if (tokens) {
        for (i = 0; *(tokens + i); i++) {
            strcpy(cv[i], trim(*(tokens + i)));
            free(*(tokens + i));
            akhir = i+1;
        }
        printf("\n");
        free(tokens);
    }

    i = 0;
    int found = 0;
    int index = 0;
    while(i < akhir) {
        if(strcmp(cv[i], word) == 0) {
            found = 1;
            index = i+1;
            break;
        }
        i++;
    }
    //Return nilai found, index dan total row pada data
    *fou = found;
    *in = index;
    *row = akhir;
}

// void getRecord(char tipe[], char table[], size_t index) { //Fungsi get data per column dengan index
//     char buf[1024], folder[1024];
//     sprintf(folder, "databases/%s/%s", user_data.setDb, table);                               
//     FILE *fp = fopen (folder, "r");     

//     while (fgets (buf, 1024, fp)) { 
//         char *p = buf;
//         size_t i = 0;

//         for (p = strtok(p, DELIM); p && i < index; p = strtok (NULL, DELIM))
//             i++;
//         if (i == index && p) { /* Jika index ditemukan */
//             //Lakukan proses data
//             if(strcmp(tipe, "DROP") == 0) { //Belum selesai
//                 //dropDataCol(p);
//                 puts(p);
//             } else if (strcmp(tipe, "SELECT") == 0) { 
//                 puts(p);
//             }
//         } else {              /* handle error */
//             fputs ("Column tidak ada\n", stderr);
//             break;
//         }
//     }
// }

void deleteColumn(char table[], int in, int totalRow) {
    FILE *fp;
    char line[1024], buff[1024], folder[1024], delete[1024];
    char *buffer;
    char *ptr;

    buffer = (char *)malloc(1000 * sizeof(char));
    memset(buffer, 0, 1000 * sizeof(char));
    ptr = buffer;

    sprintf(folder, "databases/%s/%s", user_data.setDb, table);
    fp = fopen(folder, "r");
    if (fp != NULL) {
		int row = 0;
		int column = 0;
        int newline = 2;
        int first = 1;
		while (fgets(buff, 1024, fp)) {
			column = 0;
			row++;

			char* value = strtok(buff, ",");
			while (value != NULL) {

				if (column == in-1) { 
                    strcpy(delete, value);
                // } else if(column+1 == totalRow) { 
                //      if(row == newline){
                //         sprintf(ptr, "\n%s", value);
                //         ptr += strlen(value);
                //         newline = row+1;
                //     } else if(first == 1) {
                //         sprintf(ptr, "%s", value);
                //         ptr += strlen(value);
                //         newline = row+1;
                //         first++;
                //     } else {
                //         sprintf(ptr, ",%s", value);
                //         ptr += strlen(value) + 1;
                //     }
                } else {
                    if(row == newline){
                        sprintf(ptr, "%s", value);
                        ptr += strlen(value);
                        newline = row+1;
                    } else if(first == 1) {
                        sprintf(ptr, "%s", value);
                        ptr += strlen(value);
                        newline = row+1;
                        first++;
                    } else {
                        sprintf(ptr, ",%s", value);
                        ptr += strlen(value) + 1;
                    }
                }
				value = strtok(NULL, ",");
				column++;
			}
		}
        fclose(fp);
        fp = fopen(folder, "w");
        fprintf(fp, "%s", buffer);
        fclose(fp);
    } else {
        printf("File gagal dibuka");
    }
    message("\nColumn berhasil dihapus.");  
}

void dropColumn(char query[]) {
    int loop = 1;
    //Lakukan split query yang digunakan
    char *p = strtok(query, " ");
    char column[1024];
    char tipe[1024], table[1024];
    
    while ( p ) {
        if(loop == 1) {
            sprintf(tipe, "%s", p);
        } else if(loop == 3) {
            strcpy(column, p);
        } else if(loop == 5) {
            strtok(p, ";");
            sprintf(table, "%s.csv", p);
        }
        p = strtok(NULL, " ");
        loop++;
    }
    if(strcmp(user_data.setDb, "kosong") != 0) { //Jika db sudah di use
        if (TableExist(table) == 1) { //Jika table ada
            int in, fo, row;
            //puts("Drop Column");
            GetColumn(table, column, &in, &fo, &row);
            if (fo == 1) {
                deleteColumn(table, in, row);
                message("\nColumn berhasil dihapus.");  
            } else if(fo == 0) {
                printf("Column Tidak ditemukan");
            }
        } else {
            message("\nColumn berhasil dihapus.");
        }
    } else {
        message("\nSilahkan set database terlebih dahulu.");
    }
}

void drop(char query[]) {
    int loop = 1;
    char tipe[1024], buffer[1024];
    strcpy(buffer, query);
    char* value = strtok(buffer, " ");
    while (value != NULL) {
        if (loop == 2) {
            sprintf(tipe, "%s", value);
        }
        value = strtok(NULL, " ");
		loop++;
    }
    if (strcmp(tipe, "DATABASE") == 0) { //Jika tipe adalah USER
        dropDb(query);
    } else if (strcmp(tipe, "TABLE") == 0) {
        dropTable(query);
    } else if (strcmp(tipe, "COLUMN") == 0) { //Belum Selesai
        dropColumn(query);
    }
}

void delete(char query[]) {

}

void loginsukses()
{
    char msg[1024], buffer[1024], loc[1024];
    //Tampilkan pesan awal
    // message("\e[1555557;1H\e[2J\n");
    printf("\nUser %s telah berhasil login.", user_data.name);
    sprintf(msg, "Selamat datang, %s!", user_data.name);
    message(msg);
    // sprintf(loc, "db@%s: ", user_data.name);
    // message(loc);
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
            catatLog(buffer);
            create(buffer);
        } else if (strcmp(cmd, "USE") == 0) {
            catatLog(buffer);
            useDb(buffer);
        } else if (strcmp(cmd, "GRANT") == 0) {
            if(strcmp(user_data.name, "root") == 0) {
                catatLog(buffer);
                ChangePermission(buffer);
                message("Permission telah dirubah");
            } else {
                message("Anda tidak diizinkan untuk mengganti permission");
            }
        } else if(strcmp(cmd, "DROP") == 0) {
            catatLog(buffer);
            drop(buffer);
        } else if(strcmp(cmd, "INSERT") == 0){
            catatLog(buffer);
            insert(buffer);
        } else if(strcmp(cmd, "DELETE") == 0) {
            catatLog(buffer);
            delete(buffer);
        } else {
            message("Query invalid");
        }
    }
}

//Fungsi untuk melakukan login
int login(char id[], char password[])
{
    strcpy(id, trim(id));
    strcpy(password, trim(password));
    FILE *fp = fopen("databases/auth/akun.csv", "r"); //Buka file akun
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
    printf("%d", is_auth);
    return is_auth;
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

            if (strcmp(username, "root") == 0 || strcmp(password, "12345") == 0) {
                user_data.is_auth = 1; //set auth
                strcpy(user_data.name, username);
                strcpy(user_data.pwd, password);
            } else {
                user_data.is_auth = login(username, password);
                if (user_data.is_auth == 0) {
                    message("Login Gagal, Silahkan login ulang.");
                    //exit(0);
                } else if (user_data.is_auth == 1) {
                    strcpy(user_data.name, username);
                    strcpy(user_data.pwd, password);
                    loginsukses();
                }
            }
        } else if (user_data.is_auth == 1) { //Jika berhasil login maka ke process selanjutnya.
            //message("\e[1;1H\e[2J");
            //set user data dan password
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
    strcpy(user_data.setDb, "kosong");

    pthread_create(&input, NULL, &input_main, 0);
    while (1)
    { //Agar terus berjalan
        if (pthread_join(input, NULL) == 0)
        {
            pthread_create(&input, NULL, &input_main, 0);
        }
    }
}

char *strrep(const char *s1, const char *s2, const char *s3)
{
    if (!s1 || !s2 || !s3)
        return 0;
    size_t s1_len = strlen(s1);
    if (!s1_len)
        return (char *)s1;
    size_t s2_len = strlen(s2);
    if (!s2_len)
        return (char *)s1;

    /*
     * Two-pass approach: figure out how much space to allocate for
     * the new string, pre-allocate it, then perform replacement(s).
     */

    size_t count = 0;
    const char *p = s1;
    assert(s2_len); /* otherwise, strstr(s1,s2) will return s1. */
    do {
        p = strstr(p, s2);
        if (p) {
            p += s2_len;
            ++count;
        }
    } while (p);

    if (!count)
        return (char *)s1;

    /*
     * The following size arithmetic is extremely cautious, to guard
     * against size_t overflows.
     */
    assert(s1_len >= count * s2_len);
    assert(count);
    size_t s1_without_s2_len = s1_len - count * s2_len;
    size_t s3_len = strlen(s3);
    size_t s1_with_s3_len = s1_without_s2_len + count * s3_len;
    if (s3_len &&
        ((s1_with_s3_len <= s1_without_s2_len) || (s1_with_s3_len + 1 == 0)))
        /* Overflow. */
        return 0;
    
    char *s1_with_s3 = (char *)malloc(s1_with_s3_len + 1); /* w/ terminator */
    if (!s1_with_s3)
        /* ENOMEM, but no good way to signal it. */
        return 0;
    
    char *dst = s1_with_s3;
    const char *start_substr = s1;
    size_t i;
    for (i = 0; i != count; ++i) {
        const char *end_substr = strstr(start_substr, s2);
        assert(end_substr);
        size_t substr_len = end_substr - start_substr;
        memcpy(dst, start_substr, substr_len);
        dst += substr_len;
        memcpy(dst, s3, s3_len);
        dst += s3_len;
        start_substr = end_substr + s2_len;
    }

    /* copy remainder of s1, including trailing '\0' */
    size_t remains = s1_len - (start_substr - s1) + 1;
    assert(dst + remains == s1_with_s3 + s1_with_s3_len + 1);
    memcpy(dst, start_substr, remains);
    assert(strlen(s1_with_s3) == s1_with_s3_len);
    return s1_with_s3;
}

void remove_all_chars(char* str, char c) {
    char *pr = str, *pw = str;
    while (*pr) {
        *pw = *pr++;
        pw += (*pw != c);
    }
    *pw = '\0';
}