#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PRINTABLE_CHAR_MIN 32
#define PRINTABLE_CHAR_MAX 126

unsigned short pcc_total[PRINTABLE_CHAR_MAX - PRINTABLE_CHAR_MIN + 1] = {0};
volatile sig_atomic_t sigint_received = 0;

void sigint_handler(int sig) {
    sigint_received = 1;
}

void print_statistics_and_exit() {
    for (int i = PRINTABLE_CHAR_MIN; i <= PRINTABLE_CHAR_MAX; i++) {
        if (pcc_total[i - PRINTABLE_CHAR_MIN] > 0) {
            printf("char '%c' : %hu times\n", i, pcc_total[i - PRINTABLE_CHAR_MIN]);
        }
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_fd, new_socket, port;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    signal(SIGINT, sigint_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (sigint_received) {
            print_statistics_and_exit();
        }

        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            if (errno == EINTR && sigint_received) {
                print_statistics_and_exit();
            }
            continue;
        }

        uint16_t n_bytes, c_bytes = 0;
        read(new_socket, &n_bytes, sizeof(n_bytes));
        n_bytes = ntohs(n_bytes);

        char buffer[1024];
        ssize_t bytes_read;

        while ((bytes_read = read(new_socket, buffer, sizeof(buffer))) > 0) {
            for (int i = 0; i < bytes_read; ++i) {
                if (buffer[i] >= PRINTABLE_CHAR_MIN && buffer[i] <= PRINTABLE_CHAR_MAX) {
                    c_bytes++;
                    pcc_total[buffer[i] - PRINTABLE_CHAR_MIN]++;
                }
            }
        }

        c_bytes = htons(c_bytes);
        write(new_socket, &c_bytes, sizeof(c_bytes));
        close(new_socket);
    }

    return 0;
}