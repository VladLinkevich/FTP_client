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

#define BUF_SIZE 1024

#define SERVICE_READY 220       // Сервисное закрытие контрольного соединения.
#define NEED_PASSWORD 331       // Имя пользователя верен, нужен пароль.
#define LOGIN_SUCS 230          // Пользователь вошел в систему, продолжайте. При необходимости выйдите из системы.
#define CONTROL_CLOSE 221       // Сервисное закрытие контрольного соединения.
#define PATHNAME_CREATE 257     // "PATHNAME" создан
#define PASV_MODE 227           // Переход в пассивный режим (h1,h2,h3,h4,p1, p2).
#define NO_SUCH_FILE 550        // Запрошенные меры не приняты. Файл недоступен (например, файл не найден, нет доступа).

#define RETR 1
#define PUT 2
#define PWD 3
#define LIST 4
#define CD 5
#define HELP 6
#define MKD 7
#define CDUP 9
#define QUIT 8

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

// Сообщение об ошибке
void errorReport(char* err_info) {

    printf("# %s\n", err_info);
    exit(-1);
}

// Отправка команды с параметром socket number , тегом команды и параметром команды
void sendCommand(int sock_fd, const char* cmd, const char* info) {

    char buf[BUF_SIZE] = {0};

    strcpy(buf, cmd);
    strcat(buf, info);
    strcat(buf, "\r\n");

    if (send(sock_fd, buf, strlen(buf), 0) < 0)
    { errorReport("Send command error!"); }
}

// Контрольное соединение получает ответ сервера, возвращает код ответа
int getReplyCode(int sockfd) {

    int r_code, bytes;
    char buf[BUF_SIZE] = {0}, nbuf[5] = {0};

    if ((bytes = read(sockfd, buf, BUF_SIZE - 2)) > 0) {

        r_code = atoi(buf);
        buf[bytes] = '\0';
        printf("%s", buf);

    } else { return -1; }

    if (r_code == PASV_MODE) {
                                                   // пример buffer = 227 Entering passive mode (127,0,0,1,214,19)
        char* begin = strrchr(buf, ',')+1;      // находим последнюю запятую(т.к. у нас в buffer находятся данные для пассивного мода
        char* end = strrchr(buf, ')');
        strncpy(nbuf, begin, end - begin);      // получаем часть нового порта (19)
        nbuf[end-begin] = '\0';
        data_port = atoi(nbuf);
        buf[begin-1-buf] = '\0';                   // изменяем buffer = 227 Entering passive mode (127,0,0,1,214
        end = begin - 1;
        begin = strrchr(buf, ',')+1;            // теперб в begin будет хрониться другая часть порта (214)
        strncpy(nbuf, begin, end - begin);
        nbuf[end-begin] = '\0';
        data_port += 256 * atoi(nbuf);
    }

    return r_code;
}

// Подключение к серверу с параметрами IP-адрес и номер порта, который возвращает дескриптор, если соединение было успешным
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




// Вход пользователя в систему
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

// Определение команды пользователя
int cmdToNum(char* cmd) {

    cmd[strlen(cmd)-1] = '\0';
    if (strncmp(cmd, "retr", 4) == 0)                       { return RETR; }
    if (strncmp(cmd, "put", 3) == 0)                        { return PUT;  }
    if (strcmp(cmd, "pwd")     == 0)                        { return PWD;  }
    if (strcmp(cmd, "list")    == 0)                        { return LIST; }
    if (strncmp(cmd, "mkd", 3)  == 0)                        { return MKD;  }
    if (strcmp(cmd, "cdup")  == 0)                        { return CDUP; }
    if (strncmp(cmd, "cd", 2)  == 0)                        { return CD;   }
    if (strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0)  { return HELP; }
    if (strcmp(cmd, "quit") == 0)                           { return QUIT; }

    return -1;  // No command
}

// Загрузка одного файла с сервера
void cmd_get(int sockfd, char* cmd) {

    int i = 0;
    int data_sock;
    int bytes;

    char filename[BUF_SIZE];
    char buf[BUF_SIZE];

    while (i < strlen(cmd) && cmd[i] != ' ') i++;               // проверка на то что минимум 2 слова
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;               // проверка на то что 2 слово есть
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(filename, cmd+i, strlen(cmd+i)+1);        // заносим название фаила
                                                                // Устанавливает режим передачи данных
    sendCommand(sockfd, "TYPE ", "I");                // E - EBCDIC, A - ASCII, I - 8-bit binary
    getReplyCode(sockfd);

    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!\n");
        return;
    }

    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { errorReport("Create socket error!"); }

    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
    { errorReport("Cannot connect to server!"); }

    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
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

// Загрузите файл на сервер
void cmd_put(int sockfd, char* cmd) {
    int i = 0, data_sock, bytes;
    char filename[BUF_SIZE], buf[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(filename, cmd+i, strlen(cmd+i)+1);

    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!");
        return;
    }
    FILE* src_file;
    if ((src_file = fopen(filename, "rb")) == NULL) {
        printf("Error!");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errorReport("Create socket error!");
    }
    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    sendCommand(sockfd, "STOR ", filename);
    if (getReplyCode(sockfd) == NO_SUCH_FILE) {
        close(data_sock);
        fclose(src_file);
        return;
    }
    while ((bytes = fread(buf, 1, BUF_SIZE, src_file)) > 0)
        send(data_sock, buf, bytes, 0);

    close(data_sock);
    getReplyCode(sockfd);
    fclose(src_file);
}

// Отображение текущего каталога
void cmd_pwd(int sockfd) {
    sendCommand(sockfd, "PWD", "");
    if (getReplyCode(sockfd) != PATHNAME_CREATE)
        errorReport("Wrong reply for PWD!");
}

void cmd_cdup(int sockfd){

    sendCommand(sockfd, "CDUP", "");
    if (getReplyCode(sockfd) != 200)
        errorReport("Wrong reply for PWD!");
}

void cmd_mkd(int sockfd, char* cmd) {

    int i = 0;
    char filename[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(filename, cmd+i, strlen(cmd+i)+1);

    sendCommand(sockfd, "MKD ", filename);

}

// Список удаленных текущих каталогов
void cmd_list(int sockfd) {

    int data_sock, bytes;
    char buf[BUF_SIZE] = {0};
    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET,
            SOCK_STREAM,                           // SOCK_STREAM (надёжная потокоориентированная служба (сервис) или потоковый сокет)
            0)) == 0)                           // протокол однозначно определяется по домену и типу сокета
        { errorReport("Create socket error!"); }

    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        { errorReport("Cannot connect to server!"); }
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    sendCommand(sockfd, "LIST ", "-al");
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
void cmd_cd(int sockfd, char* cmd) {
    int i = 0;
    char buf[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(buf, cmd+i, strlen(cmd+i)+1);
    sendCommand(sockfd, "CWD ", buf);
    getReplyCode(sockfd);
}


void cmd_help() {
    printf(" retr \t get a file from server.\n");
    printf(" put \t send a file to server.\n");
    printf(" pwd \t get the present directory on server.\n");
    printf(" list \t list the directory on server.\n");
    printf(" cd \t change the directory on server.\n");
    printf(" ?/help\t help you know how to use the command.\n");
    printf(" quit \t quit client.\n");
}

// выход
void cmd_quit(int sockfd) {
    sendCommand(sockfd, "QUIT", "");
    if (getReplyCode(sockfd) == CONTROL_CLOSE)
        printf("Logout.\n");
}

// Запуск клиента
void run(char* ip, char* pt) {

    int  sockfd = connectToHost(ip, pt);
    if (getReplyCode(sockfd) != SERVICE_READY)              // (SERVICE_READY == 220)
    { errorReport("Service Connect Error!"); }

    while (userLogin(sockfd) != 0)                          // Bход в систему
    { printf("Please try again.\n"); }

    //pthread_t id;
    //if (pthread_create(&id,NULL, cmd_put, NULL)!=0){
    //    errorReport("Thread create error.");
   // }

    int isQuit = 0;
    char buf[BUF_SIZE];

    while (!isQuit) {
        printf("[Client command] ");
        fgets(buf, sizeof(buf), stdin);
        switch (cmdToNum(buf)) {
            case RETR:  cmd_get(sockfd, buf);                   break;
            case CDUP:  cmd_cdup(sockfd);                       break;
            case PUT:   cmd_put(sockfd, buf);                   break;
            case PWD:   cmd_pwd(sockfd);                        break;
            case LIST:  cmd_list(sockfd);                       break;
            case CD:    cmd_cd(sockfd, buf);                    break;
            case MKD:   cmd_mkd(sockfd, buf);                   break;
            case HELP:  cmd_help();                             break;
            case QUIT:  cmd_quit(sockfd);       isQuit = 1;     break;
            default:
                cmd_help();
                break;
        }
    }

    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <host> [<port>]\n", argv[0]);
        exit(-1);
    }
    else if (argc == 2)
        run(argv[1], "21");
    else
        run(argv[1], argv[2]);
}
