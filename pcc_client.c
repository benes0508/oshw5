#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server IP> <server port> <file path>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    long port_tmp = strtol(argv[2], NULL, 10);
    if (port_tmp <= 0 || port_tmp > USHRT_MAX) {
        fprintf(stderr, "Error: Invalid port number.\n");
        exit(1);
    }
    uint16_t server_port = (uint16_t)port_tmp;
    const char *file_path = argv[3];

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        perror("Error opening file");
        exit(1);
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(1);
    }

    if (connect(sock_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection Failed");
        exit(1);
    }

    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);
    if (file_size < 0 || file_size > 0xFFFF) {
        fprintf(stderr, "Error: File size is too large or invalid.\n");
        close(file_fd);
        close(sock_fd);
        exit(1);
    }

    uint16_t net_file_size = htons((uint16_t)file_size);
    if (write(sock_fd, &net_file_size, sizeof(net_file_size)) != sizeof(net_file_size)) {
        perror("Error sending file size");
        close(file_fd);
        close(sock_fd);
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    ssize_t read_bytes, written_bytes, total_written;
    while ((read_bytes = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        total_written = 0;
        do {
            written_bytes = write(sock_fd, buffer + total_written, read_bytes - total_written);
            if (written_bytes < 0) {
                perror("Error sending file data");
                close(file_fd);
                close(sock_fd);
                exit(1);
            }
            total_written += written_bytes;
        } while (total_written < read_bytes);
    }

    if (read_bytes < 0) {
        perror("Error reading file");
        close(file_fd);
        close(sock_fd);
        exit(1);
    }

    uint16_t net_printable_count;
    if (read(sock_fd, &net_printable_count, sizeof(net_printable_count)) != sizeof(net_printable_count)) {
        perror("Error receiving printable count");
        close(file_fd);
        close(sock_fd);
        exit(1);
    }
    uint16_t printable_count = ntohs(net_printable_count);

    printf("# of printable characters: %hu\n", printable_count);

    close(file_fd);
    close(sock_fd);
    return 0;
}
