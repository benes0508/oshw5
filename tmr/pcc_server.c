#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>


#define MAX_PRINTABLE 126
#define MIN_PRINTABLE 32
#define PRINTABLE_RANGE (MAX_PRINTABLE - MIN_PRINTABLE + 1)

volatile sig_atomic_t stop_server = 0;

unsigned short pcc_total[PRINTABLE_RANGE] = {0};

void sigint_handler(int sig) {
    stop_server = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    unsigned int port = atoi(argv[1]);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
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

    while (!stop_server) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        uint16_t n_bytes, char_count = 0;
        if (read(new_socket, &n_bytes, sizeof(n_bytes)) <= 0) {
            perror("read");
            close(new_socket);
            continue;
        }

        n_bytes = ntohs(n_bytes);
        char buffer[1024];
        ssize_t bytes_read;
        while (n_bytes > 0 && (bytes_read = read(new_socket, buffer, sizeof(buffer))) > 0) {
            n_bytes -= bytes_read;
            for (int i = 0; i < bytes_read; ++i) {
                if (buffer[i] >= MIN_PRINTABLE && buffer[i] <= MAX_PRINTABLE) {
                    char_count++;
                    pcc_total[buffer[i] - MIN_PRINTABLE]++;
                }
            }
        }

        if (bytes_read < 0) {
            perror("Error reading from socket");
            close(new_socket);
            continue;
        }

        char_count = htons(char_count);
        if (write(new_socket, &char_count, sizeof(char_count)) < 0) {
            perror("write");
        }
        close(new_socket);
    }

    for (int i = 0; i < PRINTABLE_RANGE; ++i) {
        if (pcc_total[i] > 0) {
            printf("char '%c' : %hu times\n", i + MIN_PRINTABLE, pcc_total[i]);
        }
    }

    close(server_fd);
    return 0;
}