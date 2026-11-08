#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdatomic.h>
#include <time.h>

#define BUFFER_SIZE 2048
#define MAX_CLIENTS 100

typedef struct Client {
    int fd;
    char name[32];
    struct Client *next;
} Client;

Client *clients = NULL;
int num_clients = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Contadores atômicos globais */
atomic_ullong total_bytes_received = 0;
atomic_ullong total_messages_received = 0;

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

ssize_t read_line(int fd, char *buffer, size_t capacity) {
    size_t pos = 0;
    while (pos < capacity - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0)
            return pos == 0 ? 0 : (ssize_t)pos;
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        /* Contabiliza atomicamente cada byte recebido do socket */
        atomic_fetch_add(&total_bytes_received, (unsigned long long)n);

        if (c == '\n')
            break;
        if (c != '\r')
            buffer[pos++] = c;
    }
    buffer[pos] = '\0';
    return (ssize_t)pos;
}

void broadcast(const char *message, Client *sender) {
    pthread_mutex_lock(&clients_mutex);
    Client *current = clients;
    while (current != NULL) {
        if (current != sender) {
            if (send_all(current->fd, message, strlen(message)) == -1) {
                // Silencia erros de pipe quebrado
            }
        }
        current = current->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    Client *client = arg;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 64];
    int registered = 0;

    ssize_t n = read_line(client->fd, buffer, sizeof(buffer));
    if (n <= 0 || buffer[0] == '\0') {
        close(client->fd);
        free(client);
        return NULL;
    }

    memcpy(client->name, buffer, sizeof(client->name));
    client->name[sizeof(client->name) - 1] = '\0';

    pthread_mutex_lock(&clients_mutex);
    if (num_clients >= MAX_CLIENTS) {
        pthread_mutex_unlock(&clients_mutex);
        const char *error = "Servidor cheio.\n";
        send_all(client->fd, error, strlen(error));
        close(client->fd);
        free(client);
        return NULL;
    }

    client->next = clients;
    clients = client;
    num_clients++;
    registered = 1;
    pthread_mutex_unlock(&clients_mutex);

    snprintf(message, sizeof(message), "*** %s entrou no chat. ***\n", client->name);
    broadcast(message, client);
    printf("Usuario conectado: %s (Total ativos: %d)\n", client->name, num_clients);

    while ((n = read_line(client->fd, buffer, sizeof(buffer))) > 0) {
        if (strcmp(buffer, "/quit") == 0)
            break;

        /* Incrementa o contador de mensagens recebidas com sucesso */
        atomic_fetch_add(&total_messages_received, 1);

        snprintf(message, sizeof(message), "%s: %s\n", client->name, buffer);
        broadcast(message, client);
    }

    if (registered) {
        pthread_mutex_lock(&clients_mutex);
        Client **current = &clients;
        while (*current != NULL && *current != client)
            current = &(*current)->next;

        if (*current == client) {
            *current = client->next;
            num_clients--;
        }
        pthread_mutex_unlock(&clients_mutex);

        snprintf(message, sizeof(message), "*** %s saiu do chat. ***\n", client->name);
        broadcast(message, NULL);
    }

    printf("Usuario desconectado: %s\n", client->name);
    close(client->fd);
    free(client);
    return NULL;
}

typedef struct {
    int sockfd;
} BotContext;

void *bot_thread_func(void *arg) {
    BotContext *bc = arg;
    int fd = bc->sockfd;
    free(bc);

    char buffer[BUFFER_SIZE];
    size_t pos = 0;

    while (1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) break;

        if (c == '\n') {
            buffer[pos] = '\0';

            if (strncmp(buffer, "/START_SPAM", 11) == 0) {
                int target_size = 12;
                sscanf(buffer, "/START_SPAM %d", &target_size);

                /* Garante que o tamanho de 1 byte tenha pelo menos 2 (caractere + '\n') */
                int actual_size = (target_size < 2) ? 2 : target_size;
                char *payload = malloc(actual_size + 1);
                if (payload) {
                    memset(payload, 'A', actual_size);
                    payload[actual_size - 1] = '\n';
                    payload[actual_size] = '\0';

                    time_t start_time = time(NULL);
                    while (time(NULL) - start_time < 3) {
                        if (send_all(fd, payload, actual_size) == -1) {
                            break;
                        }
                        /* Delay proporcional à metade do tamanho da mensagem */
                        if(actual_size < 1000) {
                            usleep(actual_size / 2);
                        }
                        else{
                            usleep(actual_size * 2.5);
                        }
                    }
                    free(payload);
                }
            }
            pos = 0;
        } else if (c != '\r') {
            if (pos < sizeof(buffer) - 1)
                buffer[pos++] = c;
        }
    }

    close(fd);
    return NULL;
}

void log_metrics_csv(const char *filename, int msg_size, int clients_count, 
                     unsigned long long msgs, unsigned long long bytes, double avg_size) {
    int file_exists = (access(filename, F_OK) == 0);
    FILE *f = fopen(filename, "a");
    if (!f) {
        perror("Erro ao abrir arquivo CSV");
        return;
    }

    if (!file_exists) {
        fprintf(f, "tamanho_alvo_bytes,clientes_ativos,total_mensagens,total_bytes,tamanho_medio_bytes\n");
    }

    fprintf(f, "%d,%d,%llu,%llu,%.2f\n", msg_size, clients_count, msgs, bytes, avg_size);
    fclose(f);
    printf("  [CSV] Metricas salvas em: %s\n", filename);
}

void *test_coordinator(void *arg) {
    int port = (int)(intptr_t)arg;
    sleep(2);

    int message_sizes[] = {1, 10, 100};
    int num_tests = 3;
    int bot_fds[10];

    const char *csv_filename = "metricas_teste.csv";
    remove(csv_filename);

    for (int t = 0; t < num_tests; t++) {
        int msg_size = message_sizes[t];
        printf("\n==================================================\n");
        printf(">>> [CICLO] Tamanho alvo da mensagem: %d byte(s)\n", msg_size);
        printf("==================================================\n");
        
        for (int i = 1; i <= 10; i++) {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) continue;

            struct sockaddr_in saddr;
            memset(&saddr, 0, sizeof(saddr));
            saddr.sin_family = AF_INET;
            saddr.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr);

            if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
                close(sockfd);
                continue;
            }

            char name_buf[32];
            snprintf(name_buf, sizeof(name_buf), "Bot_%d", i);
            send_all(sockfd, name_buf, strlen(name_buf));
            send_all(sockfd, "\n", 1);

            bot_fds[i - 1] = sockfd;

            BotContext *bc = malloc(sizeof(BotContext));
            if (bc) {
                bc->sockfd = sockfd;
                pthread_t b_tid;
                pthread_create(&b_tid, NULL, bot_thread_func, bc);
                pthread_detach(b_tid);
            }

            sleep(1);

            atomic_store(&total_bytes_received, 0);
            atomic_store(&total_messages_received, 0);

            printf("\n--- [TESTE] Clientes ativos: %d | Mensagem: %d bytes ---\n", i, msg_size);
            
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "/START_SPAM %d\n", msg_size);
            broadcast(cmd, NULL);

            sleep(3);

            unsigned long long bytes = atomic_load(&total_bytes_received);
            unsigned long long msgs = atomic_load(&total_messages_received);
            double avg_size = (msgs > 0) ? ((double)bytes / msgs) : 0.0;

            printf("[ESTATISTICA] Resultados (com %d clientes, msg de %d bytes):\n", i, msg_size);
            printf("  > Total de mensagens recebidas : %llu\n", msgs);
            printf("  > Total de bytes recebidos     : %llu bytes\n", bytes);
            printf("  > Tamanho medio por mensagem   : %.2f bytes\n", avg_size);

            log_metrics_csv(csv_filename, msg_size, i, msgs, bytes, avg_size);
        }

        for (int i = 0; i < 10; i++) {
            close(bot_fds[i]);
        }

        printf("\n>>> Fim do ciclo de %d bytes. Aguardando 5 segundos para o proximo ciclo...\n", msg_size);
        sleep(5);
    }

    printf("\n>>> [SUCESSO] Todos os ciclos de testes foram concluídos!\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) == -1) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Chat TCP ativo na porta %d.\n", port);

    pthread_t test_thread;
    pthread_create(&test_thread, NULL, test_coordinator, (void *)(intptr_t)port);
    pthread_detach(test_thread);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        Client *client = malloc(sizeof(Client));
        if (client == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        client->fd = client_fd;
        client->name[0] = '\0';
        client->next = NULL;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client) != 0) {
            fprintf(stderr, "Erro ao criar thread.\n");
            close(client_fd);
            free(client);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    pthread_mutex_destroy(&clients_mutex);
    return EXIT_SUCCESS;
}