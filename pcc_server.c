#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_PRINTABLE 128
#define MIN_PRINTABLE 32
#define LISTEN_QUEUE_SIZE 10

unsigned short pcc_total[MAX_PRINTABLE - MIN_PRINTABLE] = {0};
volatile sig_atomic_t sigint_received = 0;

void handle_sigint(int sig) {
    sigint_received = 1;
}

void print_statistics() {
    for (int i = 0; i < MAX_PRINTABLE - MIN_PRINTABLE; i++) {
        printf("char '%c' : %hu times\n", i + MIN_PRINTABLE, pcc_total[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int sockfd, newsockfd, portno;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[256];
    ssize_t n;
    unsigned short local_pcc[MAX_PRINTABLE - MIN_PRINTABLE] = {0};

    // Set up signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("ERROR setting signal handler");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("ERROR on setting SO_REUSEADDR");
        exit(1);
    }

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    listen(sockfd, LISTEN_QUEUE_SIZE);

    while (1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0) {
            if (errno == EINTR && sigint_received) {
                break;
            }
            perror("ERROR on accept");
            continue;
        }

        memset(local_pcc, 0, sizeof(local_pcc));
        unsigned short count = 0;
        while ((n = read(newsockfd, buffer, sizeof(buffer) - 1)) > 0) {
            for (int i = 0; i < n; i++) {
                if (buffer[i] >= MIN_PRINTABLE && buffer[i] < MAX_PRINTABLE) {
                    count++;
                    local_pcc[buffer[i] - MIN_PRINTABLE]++;
                }
            }
        }

        if (n < 0) {
            if (errno != ETIMEDOUT && errno != ECONNRESET && errno != EPIPE) {
                perror("ERROR on read");
            }
        } else {
            // Send the count back to the client.
            if (write(newsockfd, &count, sizeof(count)) < 0) {
                perror("ERROR on write");
            } else {
                // Update global statistics only on successful interaction.
                for (int i = 0; i < MAX_PRINTABLE - MIN_PRINTABLE; i++) {
                    pcc_total[i] += local_pcc[i];
                }
            }
        }

        close(newsockfd);
        if (sigint_received) {
            break;
        }
    }

    close(sockfd);
    print_statistics();
    exit(0);
}
