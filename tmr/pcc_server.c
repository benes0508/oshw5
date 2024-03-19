#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PRINTABLE_CHAR_MIN 32
#define PRINTABLE_CHAR_MAX 126

unsigned short pcc_total[PRINTABLE_CHAR_MAX - PRINTABLE_CHAR_MIN + 1] = {0};
volatile sig_atomic_t sigint_received = 0;

void handle_sigint(int sig_num) {
    sigint_received = 1;
}

void print_statistics() {
    for (int i = PRINTABLE_CHAR_MIN; i <= PRINTABLE_CHAR_MAX; i++) {
        if (pcc_total[i - PRINTABLE_CHAR_MIN] > 0) {
            printf("char '%c' : %hu times\n", i, pcc_total[i - PRINTABLE_CHAR_MIN]);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    int port = atoi(argv[1]);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (!sigint_received) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            if (errno == EINTR && sigint_received) {
                break;
            }
            perror("accept");
            continue;
        }

        uint16_t n_bytes = 0, c_bytes = 0;
        ssize_t bytes_read = read(new_socket, &n_bytes, sizeof(n_bytes));
        if (bytes_read <= 0) {
            perror("read from client");
            close(new_socket);
            continue;
        }

        n_bytes = ntohs(n_bytes);
        unsigned short received_chars[PRINTABLE_CHAR_MAX - PRINTABLE_CHAR_MIN + 1] = {0};
        char buffer[1024];
        int full_data_received = 1;

        while (n_bytes > 0) {
            bytes_read = read(new_socket, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                full_data_received = 0;
                break;
            }
            n_bytes -= bytes_read;
            for (int i = 0; i < bytes_read; ++i) {
                if (buffer[i] >= PRINTABLE_CHAR_MIN && buffer[i] <= PRINTABLE_CHAR_MAX) {
                    c_bytes++;
                    received_chars[buffer[i] - PRINTABLE_CHAR_MIN]++;
                }
            }
        }

        if (full_data_received) {
            for (int i = 0; i < PRINTABLE_CHAR_MAX - PRINTABLE_CHAR_MIN + 1; i++) {
                pcc_total[i] += received_chars[i];
            }
        }

        c_bytes = htons(c_bytes);
        write(new_socket, &c_bytes, sizeof(c_bytes));
        close(new_socket);
    }

    print_statistics();
    close(server_fd);
    return 0;
}