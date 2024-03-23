#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define MIN_CHAR_P 32
#define MAX_CHAR_P 126

volatile sig_atomic_t procClient = 0;
volatile sig_atomic_t recSigint = 0;
unsigned short countArray[MAX_CHAR_P - MIN_CHAR_P + 1] = {0};

void handle_sigint(int sig_num) 
{
    recSigint = 1; // a sigint was indeed recieved

    if (procClient == 0) 
    {
        // print stats and exit (if no other client is connected)
        for (int i = MIN_CHAR_P; i <= MAX_CHAR_P; i++) {
            if (countArray[i - MIN_CHAR_P] > 0) {
                printf("char '%c' : %hu times\n", i, countArray[i - MIN_CHAR_P]);
            }
        }
        exit(0);
    }
}

void initSigHandler() {
    struct sigaction siga;
    memset(&siga, 0, sizeof(siga));
    
    // set custom handler
    siga.sa_handler = handle_sigint; 
    
    // making sure that accept() is interrupted when needed
    siga.sa_flags = SA_RESTART; 

    sigaction(SIGINT, &siga, NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Wrong number of arguments.\n");
        exit(EXIT_FAILURE);
    }

    // making sure that we use our handler :)
    initSigHandler();


    int sfd, newSock;
    struct sockaddr_in addr;
    int opt = 1;
    
    // obtain address length
    socklen_t addrlen = sizeof(addr);

    // obtain port as an int
    int port = atoi(argv[1]);

    // init new socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sfd == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) 
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }


    // Bind! :)
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // listen 
    if (listen(sfd, 10) < 0) 
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // MAIN LOOP
    while (!recSigint) 
    {
        newSock = accept(sfd, (struct sockaddr *)&addr, &addrlen);
        if (newSock < 0) 
        {
            if (errno == EINTR && recSigint) {
                // special case handling
                break;
            }
            perror("accept");
            continue;
        }

        procClient = 1; // mark that we are proccessing a client

        uint16_t bytesExpected = 0, bytesCount = 0;
        
        ssize_t readBytes = read(newSock, &bytesExpected, sizeof(bytesExpected)); // try to read
        
        if (readBytes <= 0) 
        {
            // read failed
            perror("read from client");
            close(newSock);
            continue;
        }

        bytesExpected = ntohs(bytesExpected);
        unsigned short received_chars[MAX_CHAR_P - MIN_CHAR_P + 1] = {0};
        char buffer[1024]; 
        
        int recievedAllData = 1;


        // read and proccess
        while (bytesExpected > 0) 
        {
            readBytes = read(newSock, buffer, sizeof(buffer));
            if (readBytes <= 0) 
            {
                recievedAllData = 0; // mark that we did not in fact read all of the data
                break;
            }

            bytesExpected -= readBytes;
            
            for (int i = 0; i < readBytes; ++i) 
            { // proccess bytes recieved
                if (buffer[i] >= MIN_CHAR_P && buffer[i] <= MAX_CHAR_P) 
                {
                    bytesCount++; 
                    received_chars[buffer[i] - MIN_CHAR_P]++;
                }
            }
        }

        if (recievedAllData) 
        {    // update count
            for (int i = 0; i < MAX_CHAR_P - MIN_CHAR_P + 1; i++) 
            {
                countArray[i] += received_chars[i];
            }
        }


        // send amount of bytes read to the client and close the connection
        bytesCount = htons(bytesCount);
        write(newSock, &bytesCount, sizeof(bytesCount));
        close(newSock);

        procClient = 0; // reset flag (proccesing complete)
        if (recSigint) 
        {
            // break if signaled to
            break;
        }
    }

    // print char count
    for (int i = MIN_CHAR_P; i <= MAX_CHAR_P; i++) 
    {
        if (countArray[i - MIN_CHAR_P] > 0) 
        {
            printf("char '%c' : %hu times\n", i, countArray[i - MIN_CHAR_P]);
        }
    }
    close(sfd);
    return 0;
}