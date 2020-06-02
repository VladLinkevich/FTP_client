#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>


#define BUF_SIZE 100

enum RETURN_CODE{
    SERVICE_READY = 220,       // Сервисное закрытие контрольного соединения.
    NEED_PASSWORD = 331,       // Имя пользователя верен, нужен пароль.
    LOGIN_SUCS = 230,          // Пользователь вошел в систему, продолжайте. При необходимости выйдите из системы.
    CONTROL_CLOSE = 221,       // Сервисное закрытие контрольного соединения.
    PATHNAME_CREATE = 257,     // "PATHNAME" создан
    PASV_MODE = 227,           // Переход в пассивный режим (h1,h2,h3,h4,p1, p2).
    NO_SUCH_FILE = 550         // Запрошенные меры не приняты. Файл недоступен (например, файл не найден, нет доступа).
};

enum COMMAND{
    RETR,
    STOR,
    PWD,
    LIST,
    CWD,
    MKD,
    CDUP,
    HELP,
    QUIT
};

struct sockaddr_in server;  // разъем
/*
 *    short int          sin_family;  	 Семейство адресов
 *    unsigned short int sin_port;    	 Номер порта
 *    struct in_addr     sin_addr;    	 Адрес в интернете
 *    unsigned char      sin_zero[8];  
*/

struct hostent* hent; 
/*
 *	char FAR * h_name;		 имя хоста
 *	char FAR * FAR * h_aliases;	 дополнительные названия
 *	short h_addrtype;		 тип адреса
 *	short h_length;			 длинна каждого адреса в байтах
 *	char FAR * FAR * h_addr_list;	 список адресов
*/
char user[20];              // память для логина
char pass[20];              // память для пароля
int data_port;              // данные о порте
char source[40];


void errorReport(char*);                                // Сообщение об ошибке
enum COMMAND cmdToNum(char*);
void sendCommand(int, const char*, const char*);        // Отправка команды с параметром socket number , тегом команды и параметром команды
int getReplyCode(int);                                  // Контрольное соединение получает ответ сервера, возвращает код ответа
int connectToHost(char*, char*);                        // Подключение к серверу с параметрами IP-адрес и номер порта, который возвращает дескриптор, если соединение было успешным
int userLogin(int);                                     // Вход пользователя в систему
int checkCommand(char*);                                // Загрузка одного файла с сервера
void enteringPassiveMode(char*);
void retr(int, char*);
void pwd(int);
void cdup(int);
void mkd(int, char*);
void list(int);
void cwd(int, char*);
void help();
void quit(int);
void* stor(void*);
void run(char*, char*);                           // Запуск клиента


void errorReport(char* err_info) {

    printf("# %s\n", err_info);
    exit(-1);
}


void sendCommand(int sock_fd, const char* cmd, const char* info) {

    char buf[BUF_SIZE] = {0};

    strcpy(buf, cmd);
    strcat(buf, info);
    strcat(buf, "\r\n");

    if (send(sock_fd, buf, strlen(buf), 0) < 0)
    { errorReport("Send command error!"); }
}= 220,



int getReplyCode(int sockfd) {

    int r_code, bytes;
    char buf[BUF_SIZE] = {0}, nbuf[5] = {0};

    if ((bytes = read(sockfd, buf, BUF_SIZE - 2)) > 0) {

        r_code = atoi(buf);
        buf[bytes] = '\0';
        printf("%s", buf);

    } else { return -1; }


    if (r_code == PASV_MODE) { enteringPassiveMode(buf); }


    return r_code;
}

void enteringPassiveMode(char* command){

    char buffer[4];

    // пример buffer = 227 Entering passive mode (127,0,0,1,214,19)
    char* begin = strrchr(command, ',')+1;      // находим последнюю запятую(т.к. у нас в buffer находятся данные для пассивного мода
    char* end = strrchr(command, ')');
    strncpy(buffer, begin, end - begin);      // получаем часть нового порта (19)
    buffer[end-begin] = '\0';
    data_port = atoi(buffer);
    buffer[begin-1-buffer] = '\0';// изменяем buffer = 227 Entering passive mode (127,0,0,1,214
    end = begin - 1;
    begin = strrchr(command, ',')+1;// теперб в begin будет хрониться другая часть порта (214)
    strncpy(buffer, begin, end - begin);
    buffer[end-begin] = '\0';
    data_port += 256 * atoi(buffer);
}

int connectToHost(char* ip, char* pt) {

    int sockfd;
    int port = atoi(pt);

    if (port <= 0 || port >= 65536)
        { errorReport("Invalid Port Number!"); }

    server.sin_family = AF_INET; // константа, отвечающая за то, что устройство использует глобальную сеть по протоколу IPv4
    server.sin_port = htons(port);

    if ((server.sin_addr.s_addr = inet_addr(ip)) < 0) {

        if ((hent = gethostbyname(ip)) != 0) {
		memcpy(&server.sin_addr, hent->h_addr, sizeof(&(server.sin_addr)));
	} else  { errorReport("Invalid host!"); }
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) // создание TCP-сокета
        { errorReport("Cannot create socket."); }

    if (connect(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0) // установка соединения с сервером
        { errorReport("Cannot connect to server."); }

    printf("Successfully connect to server: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    return sockfd;
}

int pasv(int sockfd, int data_sock){


    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!");
    }

    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errorReport("Create socket error!");
    }
    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    return data_sock;

}



int userLogin(int sockfd) {

    memset(user, 0, sizeof(user));
    memset(pass, 0, sizeof(pass));

    char buf[BUF_SIZE];

    printf("Username: ");                                                // Ввод логинаа
    fgets(buf, sizeof(buf), stdin);

    if (buf[0] != '\n') { strncpy(user, buf, strlen(buf) - 1); }
    else { strncpy(user, "anonymous", 9); }

    sendCommand(sockfd, "USER ", user);                                    // отправляем логин на сервер

    if (getReplyCode(sockfd) == NEED_PASSWORD) {

        memset(buf, 0, sizeof(buf));
        printf("Password: ");

        fgets(buf, sizeof(buf), stdin);                                         // ввод пароля
        if (buf[0] != '\n') { strncpy(pass, buf, strlen(buf) - 1); }
        else { strncpy(pass, "anonymous", 9); }

        sendCommand(sockfd, "PASS ", pass);                                 // оправка пароля на сервер

        if (getReplyCode(sockfd) != LOGIN_SUCS) {
            printf("Password wrong. ");
            return -1;
        } else {
            printf("User %s login successfully!\n", user);
            return 0;
        }
    } else {
        printf("User not found! ");
        return -1;
    }
}




int checkCommand(char* cmd){

    int i = 0;
    while (i < strlen(cmd) && cmd[i] != ' ') i++;               // проверка на то что минимум 2 слова
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return -1;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;               // проверка на то что 2 слово есть
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return -1;
    }

    return i;
}


void retr(int sockfd, char* cmd) {

    int i = 0;
    int data_sock = 0;
    int bytes;

    char filename[BUF_SIZE];
    char buf[BUF_SIZE];

    if ((i = checkCommand(cmd)) == -1) { return; }

    strncpy(filename, cmd+i, strlen(cmd+i)+1);        // заносим название фаила
                                                                // Устанавливает режим передачи данных
    sendCommand(sockfd, "TYPE ", "I");                // E - EBCDIC, A - ASCII, I - 8-bit binary
    getReplyCode(sockfd);


    data_sock = pasv(sockfd, data_sock);


    sendCommand(sockfd, "RETR ", filename);                             // команда извлечения копий файла
    if (getReplyCode(sockfd) == NO_SUCH_FILE) {                              // не можем получить файл
        close(sockfd);                                                       // закрываем дескриптор
        return;
    }

    FILE* dst_file;
    if ((dst_file = fopen(filename, "wb")) == NULL) {                  // создаем новый файл
        printf("Error!");
        close(sockfd);
        return;
    }
    while ((bytes = read(data_sock, buf, BUF_SIZE)) > 0)                      //
        fwrite(buf, 1, bytes, dst_file);

    close(data_sock);
    getReplyCode(sockfd);

    fclose(dst_file);
}


// Отображение текущего каталога
void pwd(int sockfd) {
    sendCommand(sockfd, "PWD", "");
    if (getReplyCode(sockfd) != PATHNAME_CREATE)
        errorReport("Wrong reply for PWD!");
}

void cdup(int sockfd){

    sendCommand(sockfd, "CDUP", "");
    if (getReplyCode(sockfd) != 200)
        errorReport("Wrong reply for PWD!");
}

void mkd(int sockfd, char* cmd) {

    int i = 0;
    char filename[BUF_SIZE];

    if ((i = checkCommand(cmd)) == -1) { return; }

    strncpy(filename, cmd+i, strlen(cmd+i)+1);

    sendCommand(sockfd, "MKD ", filename);

    getReplyCode(sockfd);

}

// Список удаленных текущих каталогов
void list(int sockfd) {

    int data_sock = 0, bytes;
    char buf[BUF_SIZE] = {0};

    data_sock = pasv(sockfd, data_sock);

    sendCommand(sockfd, "LIST ", "");
    getReplyCode(sockfd);
    printf("\n");
                                                        // Подключение к данным получает данные, передаваемые сервером
    while ((bytes = read(data_sock, buf, BUF_SIZE - 2)) > 0) {
        buf[bytes] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    close(data_sock);
    getReplyCode(sockfd);
}



// Изменение текущего каталога издалека
void cwd(int sockfd, char* cmd) {
    int i = 0;
    char buf[BUF_SIZE];

    if ((i = checkCommand(cmd)) == -1) { return; }

    strncpy(buf, cmd+i, strlen(cmd+i)+1);
    sendCommand(sockfd, "CWD ", buf);
    getReplyCode(sockfd);
}


void help() {
    printf(" retr \t get a file from server.\n");
    printf(" stor \t send a file to server.\n");
    printf(" pwd \t get the present directory on server.\n");
    printf(" list \t list the directory on server.\n");
    printf(" cwd \t change the directory on server.\n");
    printf(" ?/help\t help you know how to use the command.\n");
    printf(" quit \t quit client.\n");
}

// выход
void quit(int sockfd) {
    sendCommand(sockfd, "QUIT", "");
    if (getReplyCode(sockfd) == CONTROL_CLOSE)
        printf("Logout.\n");
}

int sockfduser = 3;

FILE* file;


void* stor(void* data){

    int i = 0, data_sock, bytes;
    char filename[BUF_SIZE], buf[BUF_SIZE];

    if ((i = checkCommand(source)) == -1) { printf("error command line");}

    strncpy(filename, source+i, strlen(source+i)+1);


    FILE* src_file;
    if ((src_file = fopen(filename, "rb")) == NULL) {
        printf("Error!");

    }

    data_sock = pasv(sockfduser, data_sock);

    sendCommand(sockfduser, "STOR ", filename);
    if (getReplyCode(sockfduser) == NO_SUCH_FILE) {
        close(data_sock);
        fclose(src_file);
        pthread_exit(0);

    }
    while ((bytes = fread(buf, 1, BUF_SIZE, src_file)) > 0)
        send(data_sock, buf, bytes, 0);

    close(data_sock);
    getReplyCode(sockfduser);
    printf("[Client command] ");
    fclose(src_file);
    pthread_exit(0);
}

void run(char* ip, char* pt) {

    int  sockfd = connectToHost(ip, pt);
    if (getReplyCode(sockfd) != SERVICE_READY)              // (SERVICE_READY == 220)
    { errorReport("Service Connect Error!"); }

    while (userLogin(sockfd) != 0)                     // Bход в систему
    { printf("Please try again.\n"); }

    pthread_t id;
    int isQuit = 0;
    char buf[BUF_SIZE];

    while (!isQuit) {
        printf("[Client command] ");
        fgets(buf, sizeof(buf), stdin);
        switch (cmdToNum(buf)) {
            case RETR:  retr(sockfd, buf);                   break;
            case CDUP:  cdup(sockfd);                        break;
            case STOR:
                strcpy(source, buf);
              if (pthread_create(&id,NULL, stor, NULL)!=0) {
                 errorReport("Thread create error.");
             }                                              break;
            case PWD:   pwd(sockfd);                        break;
            case LIST:  list(sockfd);                       break;
            case CWD:   cwd(sockfd, buf);                   break;
            case MKD:   mkd(sockfd, buf);                   break;
            case HELP:  help();                             break;
            case QUIT:  quit(sockfd);       isQuit = 1;     break;
            default:    help();                             break;
        }
    }

    close(sockfd);
}


enum COMMAND cmdToNum(char* cmd) {

    cmd[strlen(cmd)-1] = '\0';

    if (strncmp(cmd, "retr", 4) == 0)                            { return RETR; }
    else if (strncmp(cmd, "stor", 4) == 0)                       { return STOR; }
    else if (strcmp(cmd, "pwd")     == 0)                        { return PWD;  }
    else if (strcmp(cmd, "list")    == 0)                        { return LIST; }
    else if (strncmp(cmd, "mkd", 3)  == 0)                       { return MKD;  }
    else if (strcmp(cmd, "cdup")  == 0)                          { return CDUP; }
    else if (strncmp(cmd, "cwd", 3)  == 0)                       { return CWD;  }
    else if (strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0)  { return HELP; }
    else if (strcmp(cmd, "quit") == 0)                           { return QUIT; }
    else return HELP;
}

// gcc main.c -o main -lpthread
// ./main 13.56.207.108 2000       - grinia
// ./main 127.0.0.1 5000           - local
int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <host> [<port>]\n", argv[0]);
        exit(-1);
    } else if (argc == 2) { run(argv[1], "21"); }
    else { run(argv[1], argv[2]); }
}
