#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include "http.h"
#include "files.h"
#include "mime.h"

/*Se envian todos los bytes del "buf" por el socket TCP, write() puede enviar menos bytes de los pedidos, por eso repite hasta completar "len"*/

static int write_all(int fd, const char *buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        ssize_t n = write(fd, buf + total, len - total);
        if (n <= 0)
        {
            if (n < 0 && errno == EINTR)
                continue;
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

/*Se traduce de su codigo HTTP a su frase */
static const char *status_reason(int code)
{
    switch (code)
    {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    default:
        return "Internal Server Error";
    }
}

/*Se envia una respuesta HTTP de error con un HTML, tambien tiene el Content-length y el encabezado para keep-alive*/
static void send_error_response(int fd, int code, int keep_alive)
{
    char body[256];
    char header[512];
    const char *reason = status_reason(code);

    int body_len = snprintf(body, sizeof(body),
                            "<html><head><title>%d %s</title></head>"
                            "<body><h1>%d %s</h1></body></html>\n",
                            code, reason, code, reason);

    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 %d %s\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: %s\r\n"
                           "\r\n",
                           code, reason, body_len, keep_alive ? "keep-alive" : "close");

    if (hdr_len > 0 && hdr_len < (int)sizeof(header))
    {
        write_all(fd, header, (size_t)hdr_len);
        write_all(fd, body, (size_t)body_len);
    }
}

/*Funcion de parsing y  HTTP llamada por main.c al epoll detectar datos*/
int handle_http_request(int fd)
{
    char buf[MAX_REQ_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    if (n <= 0)
    {
        return 0;
    }
    buf[n] = '\0';

    if (strstr(buf, "\r\n\r\n") == NULL && strstr(buf, "\n\n") == NULL)
    {
        send_error_response(fd, 400, 0);
        return 0;
    }

    /*Primer salto \r\n para aislar la request */
    char *line_end = strstr(buf, "\r\n");
    size_t line_len = line_end ? (size_t)(line_end - buf) : strlen(buf);
    if (line_end == NULL || line_len == 0 || line_len >= MAX_LINE_LEN)
    {
        send_error_response(fd, 400, 0);
        return 0;
    }

    char req_line[MAX_LINE_LEN];
    snprintf(req_line, sizeof(req_line), "%.*s", (int)line_len, buf);

    char method[16] = {0};
    char uri[MAX_URI_LEN + 16] = {0};
    char version[16] = {0};
    char extra[16] = {0};

    /*Extraemos las palabras y ponemos limites numericos para evitar el buffer overflow*/
    int parts = sscanf(req_line, "%15s %270s %15s %15s", method, uri, version, extra);
    if (parts != 3)
    {
        send_error_response(fd, 400, 0);
        return 0;
    }

    if (strlen(uri) >= MAX_URI_LEN || uri[0] != '/')
    {
        send_error_response(fd, 400, 0);
        return 0;
    }

    if (strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0)
    {
        send_error_response(fd, 400, 0);
        return 0;
    }

    char host[MAX_HEADER_LEN] = {0};
    char connection[MAX_HEADER_LEN] = {0};
    char user_agent[MAX_HEADER_LEN] = {0};

    /*recorremos los encabezados */
    char *cursor = line_end + 2;
    while (*cursor != '\0')
    {
        if (strncmp(cursor, "\r\n", 2) == 0)
        {
            break;
        }
        char *next_crlf = strstr(cursor, "\r\n");
        if (!next_crlf)
        {
            send_error_response(fd, 400, 0);
            return 0;
        }
        size_t hlen = (size_t)(next_crlf - cursor);
        if (hlen >= MAX_HEADER_LEN)
        {
            send_error_response(fd, 400, 0);
            return 0;
        }

        char hline[MAX_HEADER_LEN];
        snprintf(hline, sizeof(hline), "%.*s", (int)hlen, cursor);

        char *colon = strchr(hline, ':');
        if (!colon)
        {
            send_error_response(fd, 400, 0);
            return 0;
        }
        *colon = '\0';
        char *val = colon + 1;
        while (*val == ' ' || *val == '\t')
            val++;

        if (strcasecmp(hline, "Host") == 0)
        {
            snprintf(host, sizeof(host), "%s", val);
        }
        else if (strcasecmp(hline, "Connection") == 0)
        {
            snprintf(connection, sizeof(connection), "%s", val);
        }
        else if (strcasecmp(hline, "User-Agent") == 0)
        {
            snprintf(user_agent, sizeof(user_agent), "%s", val);
        }

        cursor = next_crlf + 2;
    }

    if (strcmp(version, "HTTP/1.1") == 0 && host[0] == '\0')
    {
        send_error_response(fd, 400, 0);
        return 0;
    }
    /*Vemos si la conexion queda abierta keep-alive*/
    int keep_alive = (strcmp(version, "HTTP/1.1") == 0) ? 1 : 0;
    if (strcasecmp(connection, "close") == 0)
    {
        keep_alive = 0;
    }
    else if (strcasecmp(connection, "keep-alive") == 0)
    {
        keep_alive = 1;
    }

    if (strcmp(method, "GET") != 0)
    {
        send_error_response(fd, 405, keep_alive);
        return keep_alive;
    }

    /*mandamos a files.c la validacion de seguridad con realpath los permisos y tamaño*/
    char resolved_path[PATH_MAX];
    size_t file_size = 0;
    file_status_t st = resolve_safe_path(WWW_ROOT, uri, resolved_path,
                                         sizeof(resolved_path), &file_size);

    if (st != FILE_OK)
    {
        send_error_response(fd, (int)st, keep_alive);
        return keep_alive;
    }

    int file_fd = open(resolved_path, O_RDONLY);
    if (file_fd < 0)
    {
        send_error_response(fd, 500, keep_alive);
        return keep_alive;
    }
    /*Con el content-type y content-length armamos el encabezado 200 para que el cliente sepa donde acaba el archivo en keep alive */
    const char *mime = get_mime_type(resolved_path);
    char resp_header[512];
    int hdr_len = snprintf(resp_header, sizeof(resp_header),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: %s\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: %s\r\n"
                           "\r\n",
                           mime, file_size, keep_alive ? "keep-alive" : "close");

    if (write_all(fd, resp_header, (size_t)hdr_len) < 0)
    {
        close(file_fd);
        return 0;
    }

    /*Lee el archivo en bloques de 4KB y los envia al socket sin cargar todo el archivo en RAM*/
    char file_buf[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, file_buf, sizeof(file_buf))) > 0)
    {
        if (write_all(fd, file_buf, (size_t)bytes_read) < 0)
        {
            close(file_fd);
            return 0;
        }
    }

    close(file_fd);
    return keep_alive;
}