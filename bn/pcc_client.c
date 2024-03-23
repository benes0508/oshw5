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

    if (argc != 4) 
    {
        fprintf(stderr, "Wrong number of arguements.\n");
        return 1;
    }


    // obtain port as an int
    unsigned int port = atoi(argv[2]);
    if (port == 0 || port > 65535) 
    {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }


    // init socket
    int socketNum = 0;
    struct sockaddr_in serverAddr;
    if ((socketNum = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Socket creation error");
        return 1;
    }


    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);



    // convert IP to binary
    if (inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) <= 0) 
    {
        perror("Invalid address/ Address not supported");
        return 1;
    }


    if (connect(socketNum, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) 
    {
        perror("Connection Failed");
        return 1;
    }

    // open file and obtain file descriptor
    int ffd = open(argv[3], O_RDONLY);
    if (ffd < 0) 
    {
        perror("Error opening file");
        close(socketNum);
        return 1;
    }


    // obtain file size
    off_t fsize = lseek(ffd, 0, SEEK_END);
    if (fsize < 0 || fsize > 0xFFFF) 
    {
        fprintf(stderr, "File size error or file is too large\n");
        close(ffd);
        close(socketNum);
        return 1;
    }

    // move file pointer to the beginning
    lseek(ffd, 0, SEEK_SET); 

    
    uint16_t fileSize = htons((uint16_t)fsize);
    if (write(socketNum, &fileSize, sizeof(fileSize)) < 0) 
    { 
        // if you're here it means that write() failed
        perror("Error sending file size");
        close(ffd);
        close(socketNum);
        return 1;
    }

    
    char buffer[1024];
    ssize_t rBytes, sBytes; // read bytes and sent bytes
    ssize_t totalBytesWritten = 0, bytesWritten = 0;
    

    // Send file
    while ((rBytes = read(ffd, buffer, sizeof(buffer))) > 0) 
    {
    
    totalBytesWritten = 0; // keep track on bytes written
    
    // write all bytes that were read
    while (totalBytesWritten < rBytes) {
        bytesWritten = write(socketNum, buffer + totalBytesWritten, rBytes - totalBytesWritten);
        if (bytesWritten < 0) {
            // if you're here it means that write() failed
            perror("Error sending file content");
            close(ffd);
            close(socketNum);
            return 1;
        }
        totalBytesWritten += bytesWritten;
    }
}
    if (rBytes < 0) 
    {
        // if you're here it means that write() failed
        perror("Error reading file");
        close(ffd);
        close(socketNum);
        return 1;
    }

    
    uint16_t printChars;
    if (read(socketNum, &printChars, sizeof(printChars)) < 0) {
        perror("Error reading count of printable characters");
        close(ffd);
        close(socketNum);
        return 1;
    }


    // print amount of printable characters
    printChars = ntohs(printChars);
    printf("# of printable characters: %hu\n", printChars);

    // close connection
    close(ffd);
    close(socketNum);
    return 0;
}
