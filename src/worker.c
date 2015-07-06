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

int send_response(struct connection *c)
{
    char *file_path = c->url;
    if (file_path == NULL) {
        // TODO: send 500
    }

    // TODO: sanitize url: do not allow path traversal
    // TODO: check if file exists
    // TODO: if file does not exist: answer with 404
    // TODO: if file exists: get file size, open file
    //                       respond with 200, send content-length header
    //                       use `sendfile` to send the file to the client
    // TODO: send mime-type
    // TODO: if anything fails: send 500

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
        free_connection(c);
        close(fd);
        return;
    }

    // TODO: use this as soon as it's implemented
    // if (send_response(c) < 0) {
    //     fprintf(stderr, "handle_connection: send_response failed\n");
    // }

    if (send(fd, response_ok, response_len, 0) != response_len) {
        fprintf(stderr, "handle_connection: send failed\n");
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

        if (write(ipc_sock, "", 1) != 1) {
            err_exit("Could write available-signal to socket");
        }
    }
}
