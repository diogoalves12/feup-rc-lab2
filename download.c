#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdarg.h>

#define MAX_FIELD 256
#define MAX_PATH 1024
#define BUF_SIZE 4096

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

// ---------- HELPERS DE SOCKET / FTP ----------

// lê uma linha (terminada em '\n') do socket de controlo
ssize_t ftp_read_line(int sockfd, char *buf, size_t maxlen)
{
    size_t i = 0;
    char c;
    ssize_t n;

    while (i < maxlen - 1)
    {
        n = read(sockfd, &c, 1);
        if (n <= 0)
        {
            if (i == 0)
                return n; // erro ou ligação fechada
            break;
        }
        buf[i++] = c;
        if (c == '\n')
            break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

// lê resposta FTP completa e devolve o código (220, 331, 230, 227, etc.)
int ftp_read_reply(int sockfd, char *last_line, size_t maxlen)
{
    char line[BUF_SIZE];
    int code = 0;

    while (1)
    {
        ssize_t n = ftp_read_line(sockfd, line, sizeof(line));
        if (n <= 0)
        {
            fprintf(stderr, "Erro ao ler resposta FTP\n");
            return -1;
        }

        // imprimir resposta do servidor (debug)
        printf("< %s", line);

        if (strlen(line) < 3)
            continue;

        if (sscanf(line, "%3d", &code) != 1)
            continue;

        // linha final: "xyz " (espaço a seguir ao código)
        if (line[3] == ' ')
        {
            if (last_line && maxlen > 0)
            {
                strncpy(last_line, line, maxlen - 1);
                last_line[maxlen - 1] = '\0';
            }
            break;
        }
        // se line[3] == '-', resposta multi-linha, continuar a ler
    }

    return code;
}

// envia comando FTP com CRLF no fim
int ftp_send_cmd(int sockfd, const char *fmt, ...)
{
    char cmd[BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    size_t len = strlen(cmd);
    if (len + 2 >= sizeof(cmd))
    {
        fprintf(stderr, "Erro: comando FTP demasiado longo\n");
        return -1;
    }

    cmd[len] = '\r';
    cmd[len + 1] = '\n';
    cmd[len + 2] = '\0';

    printf("> %s", cmd); // debug

    ssize_t n = write(sockfd, cmd, len + 2);
    if (n < 0)
    {
        perror("write");
        return -1;
    }

    return 0;
}

// faz connect TCP a host:port (usa gethostbyname)
int connect_tcp_host(const char *host, int port)
{
    struct hostent *h;
    if ((h = gethostbyname(host)) == NULL)
    {
        herror("gethostbyname");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr, h->h_addr_list[0], h->h_length);
    server_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// faz connect TCP a ip:port usando string IP (para data connection)
int connect_tcp_ip(const char *ip_str, int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip_str);

    if (addr.sin_addr.s_addr == INADDR_NONE)
    {
        fprintf(stderr, "IP inválido no PASV: %s\n", ip_str);
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect (data)");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// parse da resposta 227: "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)"
int parse_pasv_reply(const char *line, char *ip_str, size_t ip_len, int *port)
{
    int h1, h2, h3, h4, p1, p2;
    const char *p = strchr(line, '(');
    if (!p)
    {
        fprintf(stderr, "Erro: não encontrei '(' na resposta PASV\n");
        return -1;
    }

    if (sscanf(p + 1, "%d,%d,%d,%d,%d,%d",
               &h1, &h2, &h3, &h4, &p1, &p2) != 6)
    {
        fprintf(stderr, "Erro: falha ao fazer parse da resposta PASV\n");
        return -1;
    }

    snprintf(ip_str, ip_len, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;

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

    // 2) conectar socket de controlo (FTP porta 21)
    int ctrl_sock = connect_tcp_host(info.host, 21);
    if (ctrl_sock < 0)
    {
        fprintf(stderr, "Erro na fase: connect ao host\n");
        return 1;
    }
    printf("Ligado ao servidor FTP.\n");

    // 3) ler greeting (220 ...)
    char reply_line[BUF_SIZE];
    int code = ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));
    if (code != 220)
    {
        fprintf(stderr, "Erro na fase: greeting inicial (esperava 220, obtive %d)\n", code);
        close(ctrl_sock);
        return 1;
    }

    // 4) USER
    if (ftp_send_cmd(ctrl_sock, "USER %s", info.user) < 0)
    {
        fprintf(stderr, "Erro ao enviar USER\n");
        close(ctrl_sock);
        return 1;
    }

    code = ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));
    if (code != 230 && code != 331)
    {
        fprintf(stderr, "Erro na fase: USER (código %d)\n", code);
        close(ctrl_sock);
        return 1;
    }

    // 5) PASS (se necessário)
    if (code == 331)
    {
        if (ftp_send_cmd(ctrl_sock, "PASS %s", info.password) < 0)
        {
            fprintf(stderr, "Erro ao enviar PASS\n");
            close(ctrl_sock);
            return 1;
        }

        code = ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));
        if (code != 230)
        {
            fprintf(stderr, "Erro na fase: login (código %d)\n", code);
            close(ctrl_sock);
            return 1;
        }
    }

    // 6) TYPE I (modo binário)
    if (ftp_send_cmd(ctrl_sock, "TYPE I") < 0)
    {
        fprintf(stderr, "Erro ao enviar TYPE I\n");
        close(ctrl_sock);
        return 1;
    }

    code = ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));
    if (code != 200)
    {
        fprintf(stderr, "Erro na fase: TYPE I (código %d)\n", code);
        close(ctrl_sock);
        return 1;
    }

    // 7) PASV → obter IP/porta da ligação de dados
    if (ftp_send_cmd(ctrl_sock, "PASV") < 0)
    {
        fprintf(stderr, "Erro ao enviar PASV\n");
        close(ctrl_sock);
        return 1;
    }

    code = ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));
    if (code != 227)
    {
        fprintf(stderr, "Erro na fase: PASV (código %d)\n", code);
        close(ctrl_sock);
        return 1;
    }

    char data_ip[64];
    int data_port;
    if (parse_pasv_reply(reply_line, data_ip, sizeof(data_ip), &data_port) < 0)
    {
        fprintf(stderr, "Erro ao interpretar resposta PASV\n");
        close(ctrl_sock);
        return 1;
    }

    printf("Modo passivo: %s:%d\n", data_ip, data_port);

    // 8) abrir socket de dados
    int data_sock = connect_tcp_ip(data_ip, data_port);
    if (data_sock < 0)
    {
        fprintf(stderr, "Erro na fase: conexão de dados (PASV)\n");
        close(ctrl_sock);
        return 1;
    }

    // 9) RETR path
    if (ftp_send_cmd(ctrl_sock, "RETR %s", info.path) < 0)
    {
        fprintf(stderr, "Erro ao enviar RETR\n");
        close(data_sock);
        close(ctrl_sock);
        return 1;
    }

    code = ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));
    if (code != 150 && code != 125)
    {
        fprintf(stderr, "Erro na fase: RETR (código %d)\n", code);
        close(data_sock);
        close(ctrl_sock);
        return 1;
    }

    // 10) ler dados do data_sock e gravar em ficheiro local
    FILE *f = fopen(info.filename, "wb");
    if (!f)
    {
        perror("fopen");
        close(data_sock);
        close(ctrl_sock);
        return 1;
    }

    char buf[BUF_SIZE];
    ssize_t n;
    printf("A descarregar para '%s'...\n", info.filename);

    while ((n = read(data_sock, buf, sizeof(buf))) > 0)
    {
        if (fwrite(buf, 1, (size_t)n, f) != (size_t)n)
        {
            perror("fwrite");
            fclose(f);
            close(data_sock);
            close(ctrl_sock);
            return 1;
        }
    }

    fclose(f);
    close(data_sock);

    if (n < 0)
    {
        perror("read (data_sock)");
        close(ctrl_sock);
        return 1;
    }

    // 11) resposta final do servidor (226/250)
    code = ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));
    if (code != 226 && code != 250)
    {
        fprintf(stderr, "Aviso: código final inesperado após transferir (código %d)\n", code);
    }

    // 12) QUIT
    ftp_send_cmd(ctrl_sock, "QUIT");
    ftp_read_reply(ctrl_sock, reply_line, sizeof(reply_line));

    close(ctrl_sock);
    printf("Download completo.\n");

    return 0;
}
