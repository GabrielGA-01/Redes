
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 65536

static int wait_for_go(int fd)
{
    char line[16];
    size_t pos = 0;

    while (pos < sizeof(line) - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n == 0) return -1;

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        if (c == '\n') {
            line[pos] = '\0';
            return strcmp(line, "GO") == 0 ? 0 : -1;
        }

        if (c != '\r')
            line[pos++] = c;
    }

    return -1;
}

static double elapsed(struct timespec a, struct timespec b)
{
    return (b.tv_sec - a.tv_sec) +
           (b.tv_nsec - a.tv_nsec) / 1000000000.0;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr,
                "Uso: %s <IP> <porta> <duracao_segundos>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    int port = atoi(argv[2]);
    int duration = atoi(argv[3]);

    if (port < 1 || port > 65535 || duration <= 0) {
        fprintf(stderr, "Parametros invalidos.\n");
        return EXIT_FAILURE;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) {
        fprintf(stderr, "IP invalido.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    /* Informa ao servidor que a conexao esta pronta. */
    const char *ready = "READY\n";

    if (send(fd, ready, strlen(ready), MSG_NOSIGNAL) < 0) {
        perror("READY");
        close(fd);
        return EXIT_FAILURE;
    }

    if (wait_for_go(fd) != 0) {
        fprintf(stderr, "Falha ao receber sinal GO.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    char *buffer = malloc(BUFFER_SIZE);

    if (!buffer) {
        perror("malloc");
        close(fd);
        return EXIT_FAILURE;
    }

    memset(buffer, 'A', BUFFER_SIZE);

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    unsigned long long sent_total = 0;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (elapsed(start, now) >= duration)
            break;

        size_t offset = 0;

        while (offset < BUFFER_SIZE) {
            ssize_t n = send(fd, buffer + offset,
                             BUFFER_SIZE - offset, MSG_NOSIGNAL);

            if (n < 0) {
                if (errno == EINTR) continue;

                /* O servidor encerra a medição ao final do intervalo. */
                goto finished;
            }

            if (n == 0)
                goto finished;

            offset += (size_t)n;
            sent_total += (unsigned long long)n;
        }
    }

finished:

    shutdown(fd, SHUT_WR);
    free(buffer);
    close(fd);

    return EXIT_SUCCESS;
}
