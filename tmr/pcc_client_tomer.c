#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <IP address> <port> <file path>\n", argv[0]);
        return 1;
    }

    // Convert port from string to integer
    unsigned int port = atoi(argv[2]);
    if (port == 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    // Create socket
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 or IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return 1;
    }

    // Open file
    int file_fd = open(argv[3], O_RDONLY);
    if (file_fd < 0) {
        perror("Error opening file");
        close(sock);
        return 1;
    }

    // Determine file size
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    if (file_size < 0 || file_size > 0xFFFF) {
        fprintf(stderr, "File size error or file is too large\n");
        close(file_fd);
        close(sock);
        return 1;
    }
    lseek(file_fd, 0, SEEK_SET); // Reset file pointer to the beginning

    // Send file size to server
    uint16_t net_file_size = htons((uint16_t)file_size);
    if (write(sock, &net_file_size, sizeof(net_file_size)) < 0) {
        perror("Error sending file size");
        close(file_fd);
        close(sock);
        return 1;
    }

    // Send file content to server
    char buffer[1024];
    ssize_t bytes_read, bytes_sent;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        bytes_sent = write(sock, buffer, bytes_read);
        if (bytes_sent < 0) {
            perror("Error sending file content");
            close(file_fd);
            close(sock);
            return 1;
        }
    }
    if (bytes_read < 0) {
        perror("Error reading file");
        close(file_fd);
        close(sock);
        return 1;
    }

    // Receive and print the count of printable characters
    uint16_t printable_chars;
    if (read(sock, &printable_chars, sizeof(printable_chars)) < 0) {
        perror("Error reading count of printable characters");
        close(file_fd);
        close(sock);
        return 1;
    }
    printable_chars = ntohs(printable_chars);
    printf("# of printable characters: %hu\n", printable_chars);

    close(file_fd);
    close(sock);
    return 0;
}
