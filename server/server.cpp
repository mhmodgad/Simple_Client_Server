//
// Created by SourcesNet on 12/21/2022.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#define PORT "8082"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold
#define MAXDATASIZE 2048 // max number of bytes we can get at once
#define MAX_COMMANDS_SPACES 100
#define END "\\r\\n\n"
void get_command(int new_fd, char buffer[2048]);
char * parse_command(int new_fd, char *buf, char (*parsed)[1024]);
char * handle_get(int new_fd, char *path);
char * read_file(char *path);

void write_file(char path[1024], char data[1024]);

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int main(void) {
    char buf[MAXDATASIZE];
    char parsed[MAX_COMMANDS_SPACES][1024];
    int sockfd, new_fd, numbytes; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    /*char* buff = read_file("C:\\Users\\SourcesNet\\Desktop\\assigment1_test.txt");
    printf("the returned buffer is  %s", buff);
    fflush(stdout);*/

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
            == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
            == -1) {
            perror("setsockopt");
            exit(1);
        }

        const int enable = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
            printf("setsockopt(SO_REUSEADDR) failed");

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while (1) {  // main accept() loop
        //printf("iam here");
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr*) &their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            char buffer[2048];
            get_command(new_fd, buffer);
            printf("Server: received '%s'\n", buffer);
            char* buff = parse_command(new_fd, buffer, parsed);
            printf("\nthe returned buff is %s", buff);
            //printf("%s", parsed[0]);

            //printf("%s", parsed[1]);
            //printf("%s", parsed[2]);
            //printf("%s", parsed[3]);
            //read_file(parsed[1]);

            if (send(new_fd, buff, strlen(buff), 0) == -1)
                perror("send");
            close(new_fd);
            printf("\n\naccheived here");
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
        //break;
        //close(sockfd);
    }

    return 0;
}

void get_command(int new_fd, char buffer[2048]) {
    int numbytes;
    char received[100][2048];
    int i = 0;
    char command[2048];
    do {
        if ((numbytes = recv(new_fd, command, MAXDATASIZE - 1, 0)) == -1) {
            perror("recv");
            exit(1);
        }
        if (i == 0) {
            printf("The receved command is %s", command);
            strcpy(buffer, command);
        }
        command[numbytes] = '\0';
        i++;
    } while (strcmp(command, END) != 0);
    printf("received request\n");
}

char * parse_command(int new_fd, char *buf, char (*parsed)[1024]) {
    char *token;
    char *rest = buf;
    int i = 0;
    while ((token = strtok_r(rest, " ", &rest))) {
        strcpy(parsed[i], token);
        printf("%s ", parsed[i]);
        i++;
    }
    printf("%s ", parsed[0]);
    if (strcmp(parsed[0], "GET") == 0) {
        char* buff = handle_get(new_fd, parsed[1]);
        return buff;
    } else if (strcmp(parsed[0], "POST") == 0) {
        printf("\npath is %s", parsed[1]);
        printf("\ndata is %s", parsed[3]);
        write_file(parsed[1], parsed[3]);
        return "HTTP/1.1 200 OK\\r\\n";
    } else {
        printf("UNKNOWN REQUEST");
        exit(-1);
    }
}

void write_file(char path[1024], char data[1024]) {
    FILE *fileptr;
    fileptr = fopen(path, "w");

    if (fileptr == NULL){
        printf("unable to create file ");
        exit(EXIT_FAILURE);
    }

    fputs(data, fileptr);
    fclose(fileptr);

}

char * handle_get(int new_fd, char *path) {
    printf("in handle get");
    if (access(path, F_OK) == 0) {
        // file exists
        char* buff =  read_file(path);
        return buff;
    } else {
        return "HTTP/1.1 404 Not Found\\r\\n";
        // file doesn't exist
        //printf("CANNOT BE FOUND");
        //exit(-1);
    }
}

char * read_file(char *path) {
    printf("iam here");
    fflush(stdout);
    FILE *fileptr;
    char* buffer;
    long filelen;

    //printf("\npath is %s", &path);
    fileptr = fopen(path, "rb");  // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
    filelen = ftell(fileptr);         // Get the current byte offset in the file
    rewind(fileptr);                   // Jump back to the beginning of the file

    buffer = (char*) malloc(filelen * sizeof(char)); // Enough memory for the file
    fread(buffer, filelen, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file
    //printf("%s", buffer);
    //fflush(stdout);
    //printf("triggered");
    char * response = "HTTP/1.1 200 OK\\r\\n ";
    char copiedBuffer[1024];
    strcpy(copiedBuffer, buffer);
    char modifiedBuffer[1024];
    strcpy(modifiedBuffer, response);
    //strcpy(modifiesBuffer, buffer);
    strcat(modifiedBuffer, copiedBuffer);
    char* finalBuffer;
    finalBuffer = modifiedBuffer;
    //strcpy(finalBuffer, modifiesBuffer);
    printf("\nfinal buffer is %s", finalBuffer);
    fflush(stdout);
    return finalBuffer;
}
