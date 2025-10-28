#include "http.h"

// remove qualquer "../" e normaliza barra inicial
static int sanitize_path(char *p)
{
    if (strstr(p, ".."))
        return -1;
    if (p[0] == '\0')
    {
        p[0] = '/';
        p[1] = '\0';
    }
    if (p[0] != '/')
    {
        size_t n = strlen(p);
        memmove(p + 1, p, n + 1);
        p[0] = '/';
    }
    return 0;
}

// envia string formatada pro socket
static void send_str(int c, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    send(c, buf, strlen(buf), 0);
}

// gera listagem HTML simples de um diretório
static void list_dir(int c, const char *fs_dir, const char *req_path)
{
    DIR *d = opendir(fs_dir);
    if (!d)
    {
        send_str(c, "HTTP/1.0 403 Forbidden\r\n\r\n");
        return;
    }

    send_str(c,
             "HTTP/1.0 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Connection: close\r\n\r\n");

    send_str(c, "<!doctype html><meta charset='utf-8'>");
    send_str(c, "<h1>Index of %s</h1><ul>", req_path);

    struct dirent *e;
    while ((e = readdir(d)))
    {
        if (!strcmp(e->d_name, "."))
            continue;
        // monta link relativo: req_path + nome
        send_str(c, "<li><a href=\"%s%s%s\">%s</a></li>",
                 req_path,
                 req_path[strlen(req_path) - 1] == '/' ? "" : "/",
                 e->d_name,
                 e->d_name);
    }
    send_str(c, "</ul>");
    closedir(d);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "uso: %s <porta> <docroot>\n", argv[0]);
        return 2;
    }
    int porta = atoi(argv[1]);
    const char *docroot = argv[2];

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        perror("socket");
        return 2;
    }

    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(porta);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 2;
    }
    if (listen(s, 10) < 0)
    {
        perror("listen");
        return 2;
    }

    printf("Servidor ouvindo na porta %d, docroot=%s\n", porta, docroot);

    while (1)
    {
        int c = accept(s, NULL, NULL);
        if (c < 0)
            continue;

        // lê os primeiros bytes do pedido
        char req[4096];
        ssize_t n = recv(c, req, sizeof(req) - 1, 0);
        if (n <= 0)
        {
            close(c);
            continue;
        }
        req[n] = 0;

        // extrai método, caminho e versão da 1ª linha
        // exemplo: "GET /index.html HTTP/1.1"
        char method[8] = {0}, path[2048] = {0}, version[16] = {0};
        if (sscanf(req, "%7s %2047s %15s", method, path, version) != 3)
        {
            dprintf(c, "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n");
            close(c);
            continue;
        }

        if (strcmp(method, "GET") != 0)
        {
            dprintf(c, "HTTP/1.0 405 Method Not Allowed\r\nConnection: close\r\n\r\n");
            close(c);
            continue;
        }

        // normaliza o caminho e impede traversal
        if (sanitize_path(path) != 0)
        {
            dprintf(c, "HTTP/1.0 403 Forbidden\r\nConnection: close\r\n\r\n");
            close(c);
            continue;
        }

        // se pedir "/" ⇒ serve "/index.html"
        if (strcmp(path, "/") == 0)
            strcpy(path, "/index.html");

        char fs_path[4096];
        snprintf(fs_path, sizeof fs_path, "%s%s", docroot, path);

        // checa se é diretório/arquivo
        struct stat st;
        if (stat(fs_path, &st) == 0 && S_ISDIR(st.st_mode))
        {
            char indexp[4096];
            const char *suffix = "/index.html";

            size_t len_fs = strnlen(fs_path, sizeof(fs_path));
            size_t need = len_fs + strlen(suffix) + 1;

            if (need > sizeof(indexp))
            {
                dprintf(c, "HTTP/1.0 414 Request-URI Too Large\r\nConnection: close\r\n\r\n");
                close(c);
                continue;
            }

            snprintf(indexp, sizeof(indexp), "%s%s", fs_path, suffix);

            if (stat(indexp, &st) == 0)
            {
                // existe index.html → envia index
                int fd = open(indexp, O_RDONLY);
                if (fd < 0)
                {
                    dprintf(c, "HTTP/1.0 500 Internal Server Error\r\n\r\n");
                    close(c);
                    continue;
                }
                const char *mime = "text/html; charset=utf-8";
                dprintf(c, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n",
                        mime, (long long)st.st_size);
                char buf[8192];
                ssize_t r;
                while ((r = read(fd, buf, sizeof buf)) > 0)
                    send(c, buf, r, 0);
                close(fd);
            }
            else
            {
                // não tem index.html → lista diretório
                list_dir(c, fs_path, path);
            }
            close(c);
            continue;
        }

        // não é diretório: tenta abrir como arquivo normal
        int fd = open(fs_path, O_RDONLY);
        if (fd < 0)
        {
            dprintf(c, "HTTP/1.0 404 Not Found\r\nConnection: close\r\n\r\n");
            close(c);
            continue;
        }
        if (fstat(fd, &st) < 0)
        {
            close(fd);
            dprintf(c, "HTTP/1.0 500 Internal Server Error\r\nConnection: close\r\n\r\n");
            close(c);
            continue;
        }
        const char *mime = guess_mime(path);
        dprintf(c, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n",
                mime, (long long)st.st_size);
        char buf[8192];
        ssize_t r;
        while ((r = read(fd, buf, sizeof buf)) > 0)
            send(c, buf, r, 0);
        close(fd);
        close(c);
    }
    return 0;
}
