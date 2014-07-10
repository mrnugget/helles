#include "fmacros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "networking.h"
#include "ipc.h"
#include "worker.h"
#include "http_parser.h"

char *response_ok = "HTTP/1.1 200 OK\r\n"
"Content-Length: 32\r\n"
"Connection: close\r\n\r\n"
"It's a UNIX system! I know this!";

struct connection {
    int complete;
    int fd;
    int bufsize;
    char buffer[BUFSIZE];
};

struct connection *new_connection(int fd)
{
    struct connection *c = malloc(sizeof(struct connection));
    if (c == NULL) {
        return NULL;
    }

    c->complete = 0;
    c->fd = fd;
    c->bufsize = BUFSIZE;
    memset(c->buffer, '\0', c->bufsize);

    return c;
}

int message_complete_cb(http_parser *p)
{
    struct connection *c = p->data;
    c->complete = 1;
    return 0;
}

static struct http_parser_settings settings = {
    .on_message_complete = message_complete_cb
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

static void handle_connection(int fd, http_parser *p)
{
    int response_len = strlen(response_ok);

    struct connection *c = new_connection(fd);
    if (c == NULL) err_exit("Could not allocate connection\n");

    p->data = c;

    if (read_request(p, c) < 0) {
        fprintf(stderr, "Could not read request\n");
        free(c);
        close(fd);
        return;
    }

    if (send(fd, response_ok, response_len, 0) != response_len) {
        fprintf(stderr, "handle_connection: send failed\n");
    }

    free(c);
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

        if (write(ipc_sock, "", 1) != 1) {
            err_exit("Could write available-signal to socket");
        }
    }
}
