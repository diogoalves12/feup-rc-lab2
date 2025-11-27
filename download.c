#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_FIELD 256
#define MAX_PATH 1024

typedef struct
{
    char user[MAX_FIELD];
    char password[MAX_FIELD];
    char host[MAX_FIELD];
    char path[MAX_PATH];      // caminho completo no servidor
    char filename[MAX_FIELD]; // última parte do path
} ftp_url_t;

// ---------- PARSE DA URL ----------

int parse_url(const char *url, ftp_url_t *info)
{
    const char *prefix = "ftp://";
    size_t prefix_len = strlen(prefix);

    if (strncmp(url, prefix, prefix_len) != 0)
    {
        fprintf(stderr, "Erro: URL tem de começar por ftp://\n");
        return -1;
    }

    const char *ptr = url + prefix_len;

    // ver se há credenciais [user:pass@]
    const char *at = strchr(ptr, '@');
    const char *slash = strchr(ptr, '/');

    if (at != NULL && (slash == NULL || at < slash))
    {
        // temos user:pass@
        char cred[MAX_FIELD * 2];
        size_t cred_len = at - ptr;
        if (cred_len >= sizeof(cred))
        {
            fprintf(stderr, "Erro: credenciais demasiado longas\n");
            return -1;
        }
        strncpy(cred, ptr, cred_len);
        cred[cred_len] = '\0';

        char *colon = strchr(cred, ':');
        if (colon == NULL)
        {
            fprintf(stderr, "Erro: formato das credenciais inválido (esperava user:pass)\n");
            return -1;
        }

        *colon = '\0';
        strncpy(info->user, cred, sizeof(info->user));
        info->user[sizeof(info->user) - 1] = '\0';

        strncpy(info->password, colon + 1, sizeof(info->password));
        info->password[sizeof(info->password) - 1] = '\0';

        ptr = at + 1; // depois do '@' começa o host
    }
    else
    {
        // sem credenciais → anonymous
        strncpy(info->user, "anonymous", sizeof(info->user));
        info->user[sizeof(info->user) - 1] = '\0';

        strncpy(info->password, "anonymous@", sizeof(info->password));
        info->password[sizeof(info->password) - 1] = '\0';
    }

    // separar host e path
    slash = strchr(ptr, '/');
    if (slash == NULL)
    {
        fprintf(stderr, "Erro: falta o path (não encontrei '/')\n");
        return -1;
    }

    size_t host_len = slash - ptr;
    if (host_len == 0 || host_len >= sizeof(info->host))
    {
        fprintf(stderr, "Erro: host inválido\n");
        return -1;
    }
    strncpy(info->host, ptr, host_len);
    info->host[host_len] = '\0';

    const char *path_ptr = slash + 1;
    if (*path_ptr == '\0')
    {
        fprintf(stderr, "Erro: path vazio\n");
        return -1;
    }
    if (strlen(path_ptr) >= sizeof(info->path))
    {
        fprintf(stderr, "Erro: path demasiado longo\n");
        return -1;
    }
    strncpy(info->path, path_ptr, sizeof(info->path));
    info->path[sizeof(info->path) - 1] = '\0';

    // filename = última parte do path
    const char *last_slash = strrchr(path_ptr, '/');
    const char *filename_ptr = (last_slash == NULL) ? path_ptr : last_slash + 1;

    if (*filename_ptr == '\0')
    {
        fprintf(stderr, "Erro: filename vazio\n");
        return -1;
    }
    if (strlen(filename_ptr) >= sizeof(info->filename))
    {
        fprintf(stderr, "Erro: filename demasiado longo\n");
        return -1;
    }
    strncpy(info->filename, filename_ptr, sizeof(info->filename));
    info->filename[sizeof(info->filename) - 1] = '\0';

    return 0;
}

// ---------- MAIN ----------

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s ftp://[user:pass@]host/path/to/file\n", argv[0]);
        return 1;
    }

    const char *url = argv[1];
    ftp_url_t info;

    // 1) Parse da URL
    if (parse_url(url, &info) != 0)
    {
        fprintf(stderr, "Falha ao interpretar a URL.\n");
        return 1;
    }

    printf("=== PARSE URL ===\n");
    printf("User:     %s\n", info.user);
    printf("Password: %s\n", info.password);
    printf("Host:     %s\n", info.host);
    printf("Path:     %s\n", info.path);
    printf("Filename: %s\n", info.filename);

    // 2) gethostbyname(host)
    struct hostent *h;
    if ((h = gethostbyname(info.host)) == NULL)
    {
        herror("gethostbyname");
        return 1;
    }

    char *ip_str = inet_ntoa(*((struct in_addr *)h->h_addr));
    printf("IP:       %s\n", ip_str);

    // 3) socket + connect à porta 21 (FTP)
    int sockfd;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(21); // porta FTP
    server_addr.sin_addr.s_addr = inet_addr(ip_str);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Ligado ao servidor FTP.\n");

    // Ler a mensagem inicial do servidor (220 ...)
    char buf[1024];
    ssize_t n = read(sockfd, buf, sizeof(buf) - 1);
    if (n < 0)
    {
        perror("read");
    }
    else if (n == 0)
    {
        printf("Servidor fechou a ligação.\n");
    }
    else
    {
        buf[n] = '\0';
        printf("Resposta inicial do servidor:\n%s", buf);
    }

    close(sockfd);
    return 0;
}
