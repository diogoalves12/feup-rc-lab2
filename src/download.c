#include "../include/download.h"

int parse(char *input, struct URL *url) {

    regex_t rx;

    // tem de ter barra
    if (regcomp(&rx, BAR, 0) != 0 || regexec(&rx, input, 0, NULL, 0) != 0)
        return -1;
    regfree(&rx);

    // ver se tem user:pass@
    regcomp(&rx, AT, 0);
    if (regexec(&rx, input, 0, NULL, 0) != 0) {
        // ftp://<host>/<url-path>
        sscanf(input, HOST_REGEX, url->host);
        strcpy(url->user, DEFAULT_USER);
        strcpy(url->password, DEFAULT_PASSWORD);
    } else {
        // ftp://<user>:<pass>@<host>/<url-path>
        sscanf(input, HOST_AT_REGEX, url->host);
        sscanf(input, USER_REGEX, url->user);
        sscanf(input, PASS_REGEX, url->password);
    }
    regfree(&rx);

    // recurso e nome do ficheiro
    sscanf(input, RESOURCE_REGEX, url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);

    if (!strlen(url->host) || !strlen(url->user) || !strlen(url->password) ||
        !strlen(url->resource) || !strlen(url->file))
        return -1;

    // resolver hostname
    struct hostent *h = gethostbyname(url->host);
    if (h == NULL) {
        printf("Invalid hostname '%s'\n", url->host);
        exit(-1);
    }
    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}

int createSocket(char *ip, int port) {

    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int readResponse(const int sock, char *buf) {

    char c;
    int i = 0, code;
    ResponseState st = START;

    memset(buf, 0, MAX_LENGTH);

    while (st != END) {
        if (read(sock, &c, 1) <= 0) break;

        switch (st) {
            case START:
                if (c == ' ') st = SINGLE;
                else if (c == '-') st = MULTIPLE;
                else if (c == '\n') st = END;
                else buf[i++] = c;
                break;

            case SINGLE:
                if (c == '\n') st = END;
                else buf[i++] = c;
                break;

            case MULTIPLE:
                if (c == '\n') {
                    memset(buf, 0, MAX_LENGTH);
                    i = 0;
                    st = START;
                } else buf[i++] = c;
                break;

            default:
                break;
        }
    }

    sscanf(buf, RESPCODE_REGEX, &code);
    return code;
}

int authConn(const int sock, const char *user, const char *pass) {

    char cmd[MAX_LENGTH];
    char resp[MAX_LENGTH];

    sprintf(cmd, "user %s\n", user);
    write(sock, cmd, strlen(cmd));
    if (readResponse(sock, resp) != SV_READY4PASS) {
        printf("Unknown user '%s'. Abort.\n", user);
        exit(-1);
    }

    sprintf(cmd, "pass %s\n", pass);
    write(sock, cmd, strlen(cmd));

    return readResponse(sock, resp);
}

int passiveMode(const int sock, char *ip, int *port) {

    char resp[MAX_LENGTH];
    int a,b,c,d,p1,p2;

    write(sock, "pasv\n", 5);
    if (readResponse(sock, resp) != SV_PASSIVE) return -1;

    sscanf(resp, PASSIVE_REGEX, &a, &b, &c, &d, &p1, &p2);
    *port = p1 * 256 + p2;
    sprintf(ip, "%d.%d.%d.%d", a, b, c, d);

    return SV_PASSIVE;
}

int requestResource(const int sock, char *res) {

    char cmd[MAX_LENGTH];
    char resp[MAX_LENGTH];

    sprintf(cmd, "retr %s\n", res);
    write(sock, cmd, strlen(cmd));

    return readResponse(sock, resp);
}

int getResource(const int ctrlSock, const int dataSock, char *filename) {

    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        printf("Error opening or creating file '%s'\n", filename);
        exit(-1);
    }

    char buf[MAX_LENGTH];
    int n;

    do {
        n = read(dataSock, buf, MAX_LENGTH);
        if (n > 0 && fwrite(buf, 1, n, f) < (size_t)n) {
            fclose(f);
            return -1;
        }
    } while (n > 0);

    fclose(f);

    char resp[MAX_LENGTH];
    return readResponse(ctrlSock, resp);
}

int closeConnection(const int ctrlSock, const int dataSock) {

    char resp[MAX_LENGTH];

    write(ctrlSock, "quit\n", 5);
    if (readResponse(ctrlSock, resp) != SV_GOODBYE) return -1;

    return close(ctrlSock) || close(dataSock);
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    struct URL url;
    memset(&url, 0, sizeof(url));

    if (parse(argv[1], &url) != 0) {
        printf("Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n",
           url.host, url.resource, url.file, url.user, url.password, url.ip);

    char resp[MAX_LENGTH];

    int sockCtrl = createSocket(url.ip, FTP_PORT);
    if (sockCtrl < 0 || readResponse(sockCtrl, resp) != SV_READY4AUTH) {
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }

    if (authConn(sockCtrl, url.user, url.password) != SV_LOGINSUCCESS) {
        printf("Authentication failed with username = '%s' and password = '%s'.\n",
               url.user, url.password);
        exit(-1);
    }

    char dataIp[MAX_LENGTH];
    int dataPort;

    if (passiveMode(sockCtrl, dataIp, &dataPort) != SV_PASSIVE) {
        printf("Passive mode failed\n");
        exit(-1);
    }

    int sockData = createSocket(dataIp, dataPort);
    if (sockData < 0) {
        printf("Socket to '%s:%d' failed\n", dataIp, dataPort);
        exit(-1);
    }

    if (requestResource(sockCtrl, url.resource) != SV_READY4TRANSFER) {
        printf("Unknown resouce '%s' in '%s:%d'\n", url.resource, dataIp, dataPort);
        exit(-1);
    }

    if (getResource(sockCtrl, sockData, url.file) != SV_TRANSFER_COMPLETE) {
        printf("Error transfering file '%s' from '%s:%d'\n", url.file, dataIp, dataPort);
        exit(-1);
    }

    if (closeConnection(sockCtrl, sockData) != 0) {
        printf("Sockets close error\n");
        exit(-1);
    }

    return 0;
}
