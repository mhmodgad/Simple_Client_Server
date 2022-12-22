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
void receive_file(int sockfd, char *path);
void read_file(int soc_fd, char *path);
void parse_command(int sockfd, char *buf, char (*parsed)[1024]);

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

		if (send(sockfd, input, strlen(input), 0) == -1) {
			perror("send");
			break;
		}
		printf("Sent request\n");
		//char buffer[2048];
		char response[10000];
		parse_command(sockfd, input, parsed);
		if (strcmp(parsed[0], "GET") == 0) {
			if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
				perror("recv");
				break;
			}
			buf[numbytes] = '\0';
			printf("client: received '%s'\n", buf);
			if (strcmp(buf, Ok) == 0) {
				receive_file(sockfd, parsed[1]);
			}
		} else if (strcmp(parsed[0], "POST") == 0) {
			read_file(sockfd, parsed[1]);
		} else if (strcmp(parsed[0], "CLOSE") == 0) {
			if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
				perror("recv");
				break;
			}
			buf[numbytes] = '\0';
			printf("client: received '%s'\n", buf);
			if (strcmp(buf, Ok) == 0) {
				printf("GOOD BYE \n");
			} else {
				printf("ERROR CLOSING \n");
			}
			break;
		} else {
			if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
				perror("recv");
				break;
			}
			buf[numbytes] = '\0';
			printf("client: received '%s'\n", buf);
		}

	}
	close(sockfd);
	return 0;
}

void parse_command(int sockfd, char *buf, char (*parsed)[1024]) {
	char *token;
	char *rest = buf;
	int i = 0;

	while ((token = strtok_r(rest, " ", &rest))) {
		strcpy(parsed[i], token);
		i++;
	}
}

void read_file(int soc_fd, char *path) {
	printf("\npath sent %s", path);
	fflush(stdout);
	FILE *fileptr;
	long filelen;
	fileptr = fopen("C:\\Users\\SourcesNet\\Documents\\Bandicam\\monster.jpg",
			"rb");  // Open the file in binary mode
	fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
	filelen = ftell(fileptr);         // Get the current byte offset in the file
	rewind(fileptr);
	char send_buffer[10000]; // no link between BUFSIZE and the file size
	int nb = fread(send_buffer, 1, sizeof(send_buffer), fileptr);
	fflush(stdout);
	while (!feof(fileptr)) {
		write(soc_fd, send_buffer, nb);
		nb = fread(send_buffer, 1, 10000, fileptr);
	}
	write(soc_fd, send_buffer, nb);
	sleep(0.001);
	write(soc_fd, Ok, strlen(Ok));
}

void receive_file(int sockfd, char *path) {
	printf("Reading Data\n");
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
	FILE *recievedFile = fopen(parsed[i - 1], "wb");
	int nb = read(sockfd, p_array, size);
	while (nb > 0) {
		if (strncmp(p_array, Ok, strlen(Ok)) == 0)
			break;
		fwrite(p_array, sizeof(char), nb, recievedFile);
		nb = read(sockfd, p_array, size);
	}
	fclose(recievedFile);
	printf("Finished reading\n");
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
