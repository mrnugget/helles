#include "fmacros.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef __linux__
#include <sys/sendfile.h>
#endif
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#include "networking.h"
#include "ipc.h"
#include "worker.h"
#include "http_parser.h"

#define MAX_HEADER_SIZE 4096
#define MAX_FILE_PATH 2048
#define SERVE_DIRECTORY "."

struct connection {
    int complete;
    int fd;
    int bufsize;
    char *buffer;
    char *url;
};

struct connection *new_connection(int fd)
{
    struct connection *c = malloc(sizeof(struct connection));
    if (c == NULL) {
        return NULL;
    }

    c->url = NULL;
    c->complete = 0;
    c->fd = fd;

    c->buffer = calloc(BUFSIZE, sizeof(char));
    if (c->buffer == NULL) {
        free(c);
        return NULL;
    }
    c->bufsize = BUFSIZE;

    return c;
}

static void free_connection(struct connection *c)
{
    if (c->buffer) {
        free(c->buffer);
    }

    if (c->url) {
        free(c->url);
    }

    free(c);
}

int url_cb(http_parser *p, const char *url, size_t url_len)
{
    struct connection *c = p->data;

    // TODO: this needs to be optimized. we don't want to call malloc while
    // handling a request. Probably use a pre-allocated buffer and limit to the
    // maximum URL size to something.
    c->url = calloc(url_len + 1, sizeof(char));
    if (c->url == NULL) return 1;

    memcpy(c->url, url, url_len);

    return 0;
}

int message_complete_cb(http_parser *p)
{
    struct connection *c = p->data;
    c->complete = 1;
    return 0;
}

static struct http_parser_settings settings = {
    .on_message_complete = message_complete_cb,
    .on_url = url_cb
};

static void err_exit(char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static int read_request(http_parser *p, struct connection *c)
{
    int nread, nparsed;

    do {
        nread = recv(c->fd, c->buffer, c->bufsize, 0);
        if (nread < 0) {
            fprintf(stderr, "Could not read from connection socket\n");
            return -1;
        }

        nparsed = http_parser_execute(p, &settings, c->buffer, nread);
        if (nparsed != nread) {
            fprintf(stderr, "nparsed != nread\n");
            return -1;
        }

        if (c->complete) break;
    } while (nread > 0);

    return 0;
}

int send_status(struct connection *c, int status_code)
{
    int status_len;
    char *status;

    switch (status_code) {
        case 200: status = "HTTP/1.1 200 OK\r\n"; break;
        case 404: status = "HTTP/1.1 404 Not Found\r\n"; break;
        case 500: status = "HTTP/1.1 500 Internal Server Error\r\n"; break;
        default:
                  fprintf(stderr, "status code %d not found\n", status_code);
                  return -1;
    }

    status_len = strlen(status);

    return send(c->fd, status, status_len, 0) == status_len;
}

int send_string(struct connection *c, char *status)
{
    int len = strlen(status);
    return send(c->fd, status, len, 0) == len;
}

int send_content_length(struct connection *c, off_t length)
{
    char header_line[MAX_HEADER_SIZE];
    sprintf(header_line, "Content-Length: %llu\r\n", (unsigned long long)length);
    return send_string(c, header_line);
}

int send_response(struct connection *c)
{
    if (c->url == NULL) {
        fprintf(stderr, "no url for connection\n");
        return -1;
    }

    // Sanity check: http_parser already cuts down the size of the URL for us.
    if ((strlen(SERVE_DIRECTORY) + strlen(c->url)) >= MAX_FILE_PATH-1) {
        fprintf(stderr, "file path exceeds buffer size\n");
        return -1;
    }

    char file_path[MAX_FILE_PATH];
    snprintf(file_path, MAX_FILE_PATH, "%s%s", SERVE_DIRECTORY, c->url);

    // TODO: sanitize url: do not allow path traversal

    if (access(file_path, F_OK) < 0) return send_status(c, 404);

    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        fprintf(stderr, "opening %s failed: %s\n", file_path, strerror(errno));
        return send_status(c, 500);
    }

    int fd = fileno(f);
    struct stat stat_buf;

    if (fstat(fd, &stat_buf) < 0) {
        fprintf(stderr, "stat %s failed: %s\n", file_path, strerror(errno));
        fclose(f);
        return send_status(c, 500);
    }

    if (send_status(c, 200) < 0) {
        fprintf(stderr, "sending status failed\n");
        fclose(f);
        return -1;
    }

    // TODO: send mime-type

    if (send_content_length(c, stat_buf.st_size) < 0) {
        fprintf(stderr, "sending content length failed\n");
        fclose(f);
        return -1;
    }

    // TODO: use a generic "send_header" function and add a `send_body` function
    // that adds the "\r\n"
    if (!send_string(c, "Connection: close\r\n\r\n")) {
        fprintf(stderr, "sending connection close failed\n");
        fclose(f);
        return -1;
    }

#ifdef __linux__
    int sr = sendfile(c->fd, fd, 0, stat_buf.st_size);
#else
    int sr = sendfile(fd, c->fd, 0, &stat_buf.st_size, NULL, 0);
#endif
    if (sr < 0) {
        perror("sendfile failed");
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

static void handle_connection(int fd, http_parser *p)
{
    struct connection *c = new_connection(fd);
    if (c == NULL) err_exit("Could not allocate connection\n");

    p->data = c;

    if (read_request(p, c) < 0) {
        fprintf(stderr, "Could not read request\n");
        free_connection(c);
        close(fd);
        return;
    }

    if (send_response(c) < 0) {
        fprintf(stderr, "handle_connection: send_response failed\n");
    }

    free_connection(c);
    close(fd);
    return;
}

void worker_loop(int ipc_sock)
{
    int recvd_conn_fd;

    http_parser *parser = malloc(sizeof(http_parser));
    if (parser == NULL) err_exit("Could not allocate parser");

    http_parser_init(parser, HTTP_REQUEST);

    for (;;) {
        if (recv_fd(ipc_sock, &recvd_conn_fd) < 0) {
            err_exit("Could not receive recvd_conn_fd");
        }

        handle_connection(recvd_conn_fd, parser);

        if (write(ipc_sock, ".", 1) != 1) {
            err_exit("Could not write available-signal to socket");
        }
    }
}
