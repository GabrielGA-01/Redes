
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 65536
#define TEST_SECONDS 10
#define REPEATS 3

static atomic_ullong total_bytes = 0;

typedef struct {
    int fd;
} ReceiverArgs;

static double elapsed(struct timespec a, struct timespec b)
{
    return (b.tv_sec - a.tv_sec) +
           (b.tv_nsec - a.tv_nsec) / 1000000000.0;
}

static int read_ready(int fd)
{
    char line[32];
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
            return strcmp(line, "READY") == 0 ? 0 : -1;
        }

        if (c != '\r')
            line[pos++] = c;
    }

    return -1;
}

static void *receive_data(void *arg)
{
    ReceiverArgs *args = arg;
    int fd = args->fd;
    free(args);

    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) return NULL;

    while (1) {
        ssize_t n = recv(fd, buffer, BUFFER_SIZE, 0);

        if (n > 0) {
            atomic_fetch_add(&total_bytes, (unsigned long long)n);
        } else if (n == 0) {
            break;
        } else if (errno != EINTR) {
            break;
        }
    }

    free(buffer);
    return NULL;
}

static void save_csv(int clients, int repeat,
                     unsigned long long bytes,
                     double duration, double mbps)
{
    FILE *f = fopen("metricas_teste.csv", "a");

    if (!f) {
        perror("CSV");
        return;
    }

    fprintf(f, "%d,%d,%llu,%.6f,%.6f\n",
            clients, repeat, bytes, duration, mbps);

    fclose(f);
}

static void wait_seconds(int seconds)
{
    struct timespec req = {seconds, 0};

    while (nanosleep(&req, &req) == -1 && errno == EINTR)
        ;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    int port = atoi(argv[1]);

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Porta invalida.\n");
        return EXIT_FAILURE;
    }

    int server = socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server);
        return EXIT_FAILURE;
    }

    if (listen(server, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server);
        return EXIT_FAILURE;
    }

    FILE *csv = fopen("metricas_teste.csv", "w");

    if (!csv) {
        perror("CSV");
        close(server);
        return EXIT_FAILURE;
    }

    fprintf(csv, "clientes,repeticao,bytes,duracao_segundos,Mbps\n");
    fclose(csv);

    printf("Servidor benchmark iniciado na porta %d\n", port);
    printf("Testes: 1 a 10 clientes | %d segundos | %d repeticoes\n",
           TEST_SECONDS, REPEATS);
    fflush(stdout);

    for (int clients = 1; clients <= MAX_CLIENTS; clients++) {

        for (int repeat = 1; repeat <= REPEATS; repeat++) {

            int sockets[MAX_CLIENTS];
            pthread_t threads[MAX_CLIENTS];
            int thread_count = 0;

            printf("\nAguardando %d clientes | Repeticao %d/%d\n",
                   clients, repeat, REPEATS);
            fflush(stdout);

            int accepted = 0;

            while (accepted < clients) {

                int fd = accept(server, NULL, NULL);

                if (fd < 0) {
                    if (errno == EINTR) continue;
                    perror("accept");
                    break;
                }

                if (read_ready(fd) != 0) {
                    fprintf(stderr, "Cliente nao enviou READY.\n");
                    close(fd);
                    continue;
                }

                sockets[accepted++] = fd;
            }

            if (accepted != clients) {
                for (int i = 0; i < accepted; i++)
                    close(sockets[i]);

                close(server);
                return EXIT_FAILURE;
            }

            atomic_store(&total_bytes, 0);

            /* Inicia todas as threads receptoras antes de liberar os clientes. */
            for (int i = 0; i < clients; i++) {
                ReceiverArgs *args = malloc(sizeof(*args));

                if (!args) break;

                args->fd = sockets[i];

                if (pthread_create(&threads[i], NULL,
                                   receive_data, args) != 0) {
                    free(args);
                    break;
                }

                thread_count++;
            }

            if (thread_count != clients) {
                for (int i = 0; i < accepted; i++)
                    shutdown(sockets[i], SHUT_RDWR);

                for (int i = 0; i < thread_count; i++)
                    pthread_join(threads[i], NULL);

                for (int i = 0; i < accepted; i++)
                    close(sockets[i]);

                close(server);
                return EXIT_FAILURE;
            }

            /* Início da medição antes da liberação dos clientes. */
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);

            int send_error = 0;

            for (int i = 0; i < clients; i++) {
                const char *go = "GO\n";

                size_t sent = 0;
                while (sent < 3) {
                    ssize_t n = send(sockets[i], go + sent,
                                     3 - sent, MSG_NOSIGNAL);

                    if (n < 0) {
                        if (errno == EINTR) continue;
                        send_error = 1;
                        break;
                    }

                    if (n == 0) {
                        send_error = 1;
                        break;
                    }

                    sent += (size_t)n;
                }
            }

            wait_seconds(TEST_SECONDS);

            clock_gettime(CLOCK_MONOTONIC, &end);

            double duration = elapsed(start, end);

            for (int i = 0; i < clients; i++)
                shutdown(sockets[i], SHUT_RDWR);

            for (int i = 0; i < thread_count; i++)
                pthread_join(threads[i], NULL);

            unsigned long long bytes = atomic_load(&total_bytes);

            double mbps = ((double)bytes * 8.0) /
                          (duration * 1000000.0);

            printf("\nRESULTADO\n");
            printf("Clientes: %d\n", clients);
            printf("Repeticao: %d\n", repeat);
            printf("Bytes recebidos: %llu\n", bytes);
            printf("Duracao: %.6f segundos\n", duration);
            printf("Taxa agregada: %.3f Mbps\n", mbps);

            if (send_error)
                printf("Aviso: houve falha ao enviar sinal GO.\n");

            save_csv(clients, repeat, bytes, duration, mbps);

            for (int i = 0; i < accepted; i++)
                close(sockets[i]);

            fflush(stdout);
        }
    }

    close(server);

    printf("\nTodos os testes foram concluidos!\n");
    printf("Arquivo gerado: metricas_teste.csv\n");

    return EXIT_SUCCESS;
}

