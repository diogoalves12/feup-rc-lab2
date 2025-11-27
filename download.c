#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ftp://[user:pass@]host/path\n", argv[0]);
        return 1;
    }

    const char *url = argv[1];
    printf("URL: %s\n", url);

    // 1) parse do URL
    // 2) gethostbyname(host)
    // 3) socket + connect porta 21
    // 4) falar FTP

    return 0;
}
