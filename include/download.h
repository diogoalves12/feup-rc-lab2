#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <termios.h>

#define MAX_LENGTH  500
#define FTP_PORT    21

/* FTP server reply codes */
#define SV_READY4AUTH           220
#define SV_READY4PASS           331
#define SV_LOGINSUCCESS         230
#define SV_PASSIVE              227
#define SV_READY4TRANSFER       150
#define SV_TRANSFER_COMPLETE    226
#define SV_GOODBYE              221

/* Regex para parsing do URL e respostas */
#define AT              "@"
#define BAR             "/"
#define HOST_REGEX      "%*[^/]//%[^/]"
#define HOST_AT_REGEX   "%*[^/]//%*[^@]@%[^/]"
#define RESOURCE_REGEX  "%*[^/]//%*[^/]/%s"
#define USER_REGEX      "%*[^/]//%[^:/]"
#define PASS_REGEX      "%*[^/]//%*[^:]:%[^@\n$]"
#define RESPCODE_REGEX  "%d"
#define PASSIVE_REGEX   "%*[^(](%d,%d,%d,%d,%d,%d)%*[^\n$)]"

/* Credenciais por omissão no caso ftp://<host>/<url-path> */
#define DEFAULT_USER        "anonymous"
#define DEFAULT_PASSWORD    "password"

/* Informação extraída do URL */
struct URL {
    char host[MAX_LENGTH];      // ex: ftp.up.pt
    char resource[MAX_LENGTH];  // ex: parrot/misc/canary/warrant-canary-0.txt
    char file[MAX_LENGTH];      // ex: warrant-canary-0.txt
    char user[MAX_LENGTH];      // username
    char password[MAX_LENGTH];  // password
    char ip[MAX_LENGTH];        // ex: 193.137.29.15
};

/* Estados para leitura de respostas do servidor */
typedef enum {
    START,
    SINGLE,
    MULTIPLE,
    END
} ResponseState;

/* Transforma o URL de input nos vários campos da struct URL */
int parse(char *input, struct URL *url);

/* Cria socket TCP ligado a ip:port e devolve o descritor */
int createSocket(char *ip, int port);

/* Autenticação: envia USER/PASS e devolve o código de resposta final */
int authConn(const int socket, const char *user, const char *pass);

/* Lê uma resposta FTP (tratando single-line e multi-line) e devolve o código */
int readResponse(const int socket, char *buffer);

/* Modo passivo: envia PASV, extrai IP e porto da ligação de dados */
int passiveMode(const int socket, char *ip, int *port);

/* Envia comando RETR para o recurso indicado */
int requestResource(const int socket, char *resource);

/* Lê os dados pela ligação de dados e grava no ficheiro local */
int getResource(const int socketA, const int socketB, char *filename);

/* Envia QUIT e fecha ambas as ligações (controlo e dados) */
int closeConnection(const int socketA, const int socketB);

#endif // DOWNLOAD_H
