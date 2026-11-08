#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#define BUFFER_SIZE 2048

int sockfd;
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

int send_all(int fd, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        sent += (size_t)n;
    }
    return 0;
}

typedef struct {
    int target_size;
} SpamArgs;

void *spam_sender_thread(void *arg) {
    SpamArgs *args = arg;
    int target_size = args->target_size;
    free(args);

    // Garante tamanho mínimo de 2 bytes para conter o caractere e o '\n' obrigatório
    int actual_size = (target_size < 2) ? 2 : target_size;

    char *payload = malloc(actual_size + 1);
    if (!payload) return NULL;

    memset(payload, 'A', actual_size);
    payload[actual_size - 1] = '\n'; // Garante quebra de linha no finalexato
    payload[actual_size] = '\0';

    time_t start_time = time(NULL);
    while (time(NULL) - start_time < 10) {
        // CORREÇÃO: Envia o bloco completo usando actual_size para incluir o '\n'
        if (send_all(sockfd, payload, actual_size) == -1) {
            break; 
        }
    }

    free(payload);
    return NULL;
}

void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    size_t pos = 0;

    (void)arg;

    while (1) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);

        if (n == 0) {
            pthread_mutex_lock(&output_mutex);
            printf("\nConexao encerrada pelo servidor.\n");
            fflush(stdout);
            pthread_mutex_unlock(&output_mutex);
            break;
        }

        if (n == -1) {
            if (errno == EINTR)
                continue;

            pthread_mutex_lock(&output_mutex);
            perror("recv");
            pthread_mutex_unlock(&output_mutex);
            break;
        }

        if (c == '\n') {
            buffer[pos] = '\0';

            if (strncmp(buffer, "/START_SPAM", 11) == 0) {
                int target_size = 12;
                sscanf(buffer, "/START_SPAM %d", &target_size);

                pthread_mutex_lock(&output_mutex);
                printf("\n[SINAL] Iniciando thread de spam por 10s (%d bytes)...\n", target_size);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);

                SpamArgs *args = malloc(sizeof(SpamArgs));
                if (args) {
                    args->target_size = target_size;
                    pthread_t t_spam;
                    if (pthread_create(&t_spam, NULL, spam_sender_thread, args) == 0) {
                        pthread_detach(t_spam);
                    } else {
                        free(args);
                    }
                }
            } else {
                pthread_mutex_lock(&output_mutex);
                printf("\r%s\n", buffer);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);
            }

            pos = 0;
        } else if (c != '\r') {
            if (pos < sizeof(buffer) - 1)
                buffer[pos++] = c;
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    char name[32];
    pthread_t receiver;

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <IP> <porta>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[2]);
    signal(SIGPIPE, SIG_IGN);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Endereco IPv4 invalido.\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Seu nome: ");
    if (fgets(name, sizeof(name), stdin) == NULL) {
        close(sockfd);
        return EXIT_FAILURE;
    }

    name[strcspn(name, "\r\n")] = '\0';

    if (name[0] == '\0' || strchr(name, '\n') != NULL) {
        fprintf(stderr, "Nome invalido.\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (send_all(sockfd, name, strlen(name)) == -1 ||
        send_all(sockfd, "\n", 1) == -1) {
        perror("send");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (pthread_create(&receiver, NULL, receive_messages, NULL) != 0) {
        fprintf(stderr, "Erro ao criar thread receptora.\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Conectado! Aguardando comandos do servidor...\n");

    pthread_join(receiver, NULL);

    close(sockfd);
    pthread_mutex_destroy(&output_mutex);
    printf("Cliente encerrado.\n");

    return EXIT_SUCCESS;
}