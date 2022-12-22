#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "8082" // the port client will be connecting to

#define MAXDATASIZE 1000000 // max number of bytes we can get at once
#define MAX_COMMANDS_SPACES 100
#define END "\\r\\n\n"
#define MAXINPUTLINES 100
char Ok[] = "HTTP/1.1 200 OK\\r\\n";
void write_file(char *path, char *data);
void get_file_data(int sockfd, char *path);
void read_file(int soc_fd, char *path);
void parse_command(int sockfd, char *buf, char (*parsed)[1024], char *response);

void handle_get(int sockfd, char string[1024], char *response);

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char parsed[MAX_COMMANDS_SPACES][1024];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char *command = NULL;
    size_t len = 0;
    ssize_t read = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
            == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*) p->ai_addr), s,
              sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    while (1) {
        char input[2048];
        memset(input, '\0', 2048 * sizeof(char));
        printf("Enter your command:\n");
        do {
            read = getline(&command, &len, stdin);
            if (read == -1)
                return -1;
            command[read] = '\0';
            strcat(input, command);
        } while (strcmp(command, END) != 0);

        printf("Completed input\n");
        if (send(sockfd, input, strlen(input), 0) == -1) {
            perror("send");
            break;
        }
        printf("Sent request\n");
        //char buffer[2048];
        char response[10000];
        parse_command(sockfd, input, parsed, response);

        if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
            perror("recv");
            break;
        }
        buf[numbytes] = '\0';
        printf("client: received '%s'\n", buf);
        if(strcmp(buf, Ok) == 0){
            get_file_data(sockfd, parsed[1]);
        }
    }
    close(sockfd);
    return 0;
}

void parse_command(int sockfd, char *buf, char (*parsed)[1024], char *response) {
    printf("\nIam here in parse command");
    fflush(stdout);
    char *token;
    char *rest = buf;
    int i = 0;

    while ((token = strtok_r(rest, " ", &rest))) {
        strcpy(parsed[i], token);
        i++;
    }

    if (strcmp(parsed[0], "GET") == 0) {
        return;
    } else if (strcmp(parsed[0], "POST") == 0) {
        printf("\nIam here in post");
        fflush(stdout);
        read_file(sockfd, parsed[1]);
        sleep(1);
        write(sockfd, Ok, strlen(Ok));
        return;
    } /*else if (strcmp(parsed[0], "CLOSE") == 0) {
        strcpy(response, "CLOSE");
    } else {
        strcpy(response, "UNKNOWN REQUEST");
    }*/
}

/*void handle_get(int sockfd, char string[1024], char *response) {
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
        perror("recv");
        break;
    }
}*/

void read_file(int soc_fd, char *path) {
    printf("\npath sent %s", path);
    fflush(stdout);
    FILE *fileptr;
    long filelen;
    fileptr = fopen("C:\\Users\\SourcesNet\\Documents\\Bandicam\\monster.jpg", "rb");  // Open the file in binary mode
    printf("\n value of nb is %d", 5);
    fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
    filelen = ftell(fileptr);         // Get the current byte offset in the file
    rewind(fileptr);
    char send_buffer[10000]; // no link between BUFSIZE and the file size
    int nb = fread(send_buffer, 1, sizeof(send_buffer), fileptr);
    fflush(stdout);
    while (!feof(fileptr)) {
        //printf("dd\n");
        write(soc_fd, send_buffer, nb);
        nb = fread(send_buffer, 1, 10000, fileptr);
    }
    printf("%d \n", nb);
    write(soc_fd, send_buffer, nb);
}

void get_file_data(int sockfd, char *path) {
    printf("Reading Picture Byte Array\n");
    int size = 10000;
    char p_array[size];

    char parsed[MAX_COMMANDS_SPACES][1024];
    char *token;
    char *rest = path;
    int i = 0;

    while ((token = strtok_r(rest, "\\", &rest))) {
        strcpy(parsed[i], token);
        i++;
    }

    printf("\n file name is %s", parsed[i-1]);
    FILE *recivedFile = fopen(parsed[i-1], "wb");
    int nb = read(sockfd, p_array, size);
    while (nb > 0) {
        if (strncmp(p_array, Ok, strlen(Ok)) == 0)
            break;

        fwrite(p_array, sizeof(char), nb, recivedFile);
        nb = read(sockfd, p_array, size);
    }
    fclose(recivedFile);
    printf("Finished reading file\n");
    fflush(stdout);
}

void write_file(char *path, char *data) {
    FILE *fileptr;
    fileptr = fopen(path, "w");

    if (fileptr == NULL) {
        printf("unable to create file ");
        exit(EXIT_FAILURE);
    }
    fputs(data, fileptr);
    fclose(fileptr);
}

//tests
//POST C:\Users\SourcesNet\Desktop\assigment2_test.txt http1.1 this_is_the_writen_code
//GET C:\Users\SourcesNet\Desktop\assigment2_test.txt http1.1
//GET E:\year3_term1\networks_labs\Assigments\Assigment1\server\monster.jpg http1.1
//POST E:\year3_term1\networks_labs\Assigments\Assigment1\server\monster.jpg http1.1
//GET C:\Users\SourcesNet\Documents\Bandicam\monster.jpg http1.1
