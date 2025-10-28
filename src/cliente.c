#include "http.h"

/* Aceita URLs do tipo: http://host[:porta]/caminho */
static void parse_url(const char *url, char host[256], char port[8], char path[1024])
{
    if (strncmp(url, "http://", 7) != 0)
    {
        fprintf(stderr, "ERRO: URL deve começar com http://\n");
        exit(2);
    }
    const char *p = url + 7;
    const char *slash = strchr(p, '/');
    const char *hostend = slash ? slash : url + strlen(url);
    const char *colon = memchr(p, ':', (size_t)(hostend - p));

    size_t hlen = (size_t)((colon ? colon : hostend) - p);
    if (hlen == 0 || hlen >= 256)
    {
        fprintf(stderr, "ERRO: host inválido\n");
        exit(2);
    }
    memcpy(host, p, hlen);
    host[hlen] = '\0';

    if (colon)
    {
        size_t plen = (size_t)(hostend - (colon + 1));
        if (plen == 0 || plen >= 8)
        {
            fprintf(stderr, "ERRO: porta inválida\n");
            exit(2);
        }
        memcpy(port, colon + 1, plen);
        port[plen] = '\0';
    }
    else
    {
        strcpy(port, "80"); // padrão HTTP
    }

    if (slash)
        snprintf(path, 1024, "%s", slash);
    else
        strcpy(path, "/");
}

static const char *basename_from_path(const char *path)
{
    const char *b = strrchr(path, '/');
    b = b ? b + 1 : path;
    if (*b == '\0')
        return "index.html"; // caminho termina com /
    return b;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "uso: %s http://host[:porta]/caminho\n", argv[0]);
        return 2;
    }

    char host[256], port[8], path[1024];
    parse_url(argv[1], host, port, path);

    // Resolve host/porta e conecta
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    int gai = getaddrinfo(host, port, &hints, &res);
    if (gai != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
        return 2;
    }

    int fd = -1;
    for (struct addrinfo *it = res; it; it = it->ai_next)
    {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0)
    {
        perror("connect");
        return 1;
    }

    // Timeout de leitura para não travar
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Envia GET HTTP/1.0 simples
    dprintf(fd,
            "GET %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "User-Agent: meu_navegador/1.0\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host);
    shutdown(fd, SHUT_WR);

    // Abre arquivo destino (a partir do path) e cria pasta se necessário.
    const char *name = basename_from_path(path);

    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "downloads/%s", name);
    system("mkdir -p downloads");

    FILE *out = fopen(fullpath, "wb");
    if (!out)
    {
        perror("fopen");
        close(fd);
        return 2;
    }

    printf("[salvando em %s]\n", fullpath);

    if (!out)
    {
        perror("fopen");
        close(fd);
        return 2;
    }

    // Lê até encontrar o fim do cabeçalho (\r\n\r\n) e depois grava o corpo
    char header[65536];
    size_t hlen = 0;
    int header_done = 0;
    int status = 0;
    char buf[8192];
    ssize_t n;

    while ((n = recv(fd, buf, sizeof buf, 0)) > 0)
    {
        if (!header_done)
        {
            // leitura do cabeçalho
            if (hlen + (size_t)n >= sizeof(header))
            {
                fprintf(stderr, "Cabeçalho HTTP muito grande\n");
                fclose(out);
                close(fd);
                return 2;
            }
            memcpy(header + hlen, buf, (size_t)n);
            hlen += (size_t)n;
            header[hlen] = '\0';
            char *p = strstr(header, "\r\n\r\n"); // procura fim do cabeçalho
            if (!p) p = strstr(header, "\n\n");
            if (p)
            {
                header_done = 1;

                // retorna status HTTP
                char *sp = strchr(header, ' ');
                if (sp)
                    status = atoi(sp + 1);

                size_t body_off = (size_t)(p + 4 - header);
                size_t body_len = hlen - body_off;
                if (body_len > 0)
                    fwrite(header + body_off, 1, body_len, out);
            }
        }
        else
        {
            // gravação do corpo
            fwrite(buf, 1, (size_t)n, out);
        }
    }

    // tratamento de erros na leitura
    if (n < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
    {
        perror("recv");
    }

    fclose(out);
    close(fd);
    struct stat st;
    
    
    /*
    // se o arquivo estiver vazio, remove (erro)
    if (stat(fullpath, &st) == 0 && st.st_size == 0)
    {
        remove(fullpath);
        fprintf(stderr, "[aviso] arquivo removido por estar vazio (erro HTTP)\n");
    }*/

    if (!header_done)
    {
        fprintf(stderr, "ERRO: resposta sem cabeçalho completo\n");
        return 2;
    }

    if (status != 200)
    {
        fprintf(stderr, "HTTP %d — arquivo salvo pode estar vazio ou parcial: %s\n", status, name);
        return 1;
    }

    printf("OK (200): salvo em '%s'\n", name);


    return 0;
}
