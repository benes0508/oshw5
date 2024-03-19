#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 12345 // This should be replaced with argv[1]
#define PRINTABLE_CHAR_MIN 32
#define PRINTABLE_CHAR_MAX 126

unsigned short pcc_total[PRINTABLE_CHAR_MAX - PRINTABLE_CHAR_MIN + 1] = {0};

void sigint_handler(int sig) {
    // Ensure this handler does not interrupt a client being processed by making it atomic if necessary
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

    unsigned int port = atoi(argv[1]);
    if (port == 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Register SIGINT handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
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

    printf("Server is listening on port %d...\n", port);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        uint16_t n_bytes, c_bytes = 0;
        if (read(new_socket, &n_bytes, sizeof(n_bytes)) <= 0) {
            perror("read");
            close(new_socket);
            continue;
        }

        n_bytes = ntohs(n_bytes);
        char buffer[1025]; // Considering a null-terminator for safe string operations if needed
        ssize_t bytes_read;
        while (n_bytes > 0 && (bytes_read = read(new_socket, buffer, sizeof(buffer) - 1)) > 0) {
            n_bytes -= bytes_read;
            for (int i = 0; i < bytes_read; ++i) {
                if (buffer[i] >= PRINTABLE_CHAR_MIN && buffer[i] <= PRINTABLE_CHAR_MAX) {
                    c_bytes++;
                    pcc_total[buffer[i] - PRINTABLE_CHAR_MIN]++;
                }
            }
        }

        if (bytes_read < 0) {
            perror("Error reading from socket");
            close(new_socket);
            continue;
        }

        c_bytes = htons(c_bytes);
        if (write(new_socket, &c_bytes, sizeof(c_bytes)) < 0) {
            perror("write");
        }

        close(new_socket);
    }

    return 0;
}
