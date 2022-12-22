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
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT "8082"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold
#define MAXDATASIZE 2048 // max number of bytes we can get at once
#define MAX_COMMANDS_SPACES 100
#define END "\\r\\n\n"

char Notfound[] = "HTTP/1.1 404 Not Found\\r\\n";
char Ok[] = "HTTP/1.1 200 OK\\r\\n";
char BadRequest[] = "HTTP/1.1 400 Bad Request\\r\\n";

void get_command(int new_fd, char buffer[2048]);
void parse_command(int new_fd, char *buf, char (*parsed)[1024]);
void handle_get(int new_fd, char *path);
void read_file(int new_fd, char *path);
void receive_file_data(int new_fd, char *path);

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

	// ftok to generate unique key
	// key_t key = ftok("shmfile", 65);

	// shmget returns an identifier in shmid
	// int shmid = shmget(key, 1024, 0666 | IPC_CREAT);

	// shmat to attach to shared memory
	/*int *num = (int*) shmat(shmid, (void*) 0, 0);
	 num = 0;*/

	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;

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
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))
				< 0)
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
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr*) &their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);
		//*num  = *num + 1;
		//printf("process: %d \n", *num);
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			while (1) {
				fd_set readfds;
				struct timeval tv;

				FD_ZERO(&readfds);

				FD_SET(new_fd, &readfds);
				tv.tv_sec = 100;
				//printf("%d \n", *num);
				rv = select(new_fd + 1, &readfds, NULL, NULL, &tv);

				if (rv == -1) {
					perror("select"); // error occurred in select()
				} else if (rv == 0) {
					printf("Timeout occurred! No data after 100 seconds.\n");
					break;
				}

				char buffer[2048];
				get_command(new_fd, buffer);
				printf("Server: received\n'%s'\n", buffer);
				parse_command(new_fd, buffer, parsed);
				if (strcmp(parsed[0], "GET") == 0) {
					handle_get(new_fd, parsed[1]);
				} else if (strcmp(parsed[0], "POST") == 0) {
					receive_file_data(new_fd, parsed[1]);
				} else if (strcmp(parsed[0], "CLOSE") == 0) {
					if (send(new_fd, Ok, strlen(Ok), 0) == -1)
						perror("send");
					break;
				} else {
					if (send(new_fd, BadRequest, strlen(BadRequest), 0) == -1)
						perror("send");
				}
			}
			printf("Closing...\n");
			fflush(stdout);
			//*num = *num - 1;
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

void get_command(int new_fd, char buffer[2048]) {
	int numbytes;
	int i = 0;
	char command[2048];
	memset(command, '\0', 2048 * sizeof(char));
	if ((numbytes = recv(new_fd, command, 2048 - 1, 0)) == -1) {
		perror("recv");
		exit(1);
	}
	strcpy(buffer, command);
	command[numbytes] = '\0';
	printf("received request\n");
	fflush(stdout);
}

void parse_command(int new_fd, char *buf, char (*parsed)[1024]) {
	char *token;
	char *rest = buf;
	int i = 0;

	while ((token = strtok_r(rest, " ", &rest))) {
		strcpy(parsed[i], token);
		i++;
	}
}

void write_file(char path[1024], char data[1024]) {
	FILE *fileptr;
	fileptr = fopen(path, "w");

	if (fileptr == NULL) {
		printf("unable to create file ");
		exit(EXIT_FAILURE);
	}

	fputs(data, fileptr);
	fclose(fileptr);

}

void handle_get(int new_fd, char *path) {
	if (access(path, F_OK) == 0) {
		if (send(new_fd, Ok, strlen(Ok), 0) == -1)
			perror("send");
		read_file(new_fd, path);
		sleep(1);
		write(new_fd, Ok, strlen(Ok));
	} else {
		if (send(new_fd, Notfound, strlen(Notfound), 0) == -1)
			perror("send");
	}
}

void read_file(int new_fd, char *path) {
	FILE *fileptr;
	long filelen;
	fileptr = fopen(path, "rb");  // Open the file in binary mode
	fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
	filelen = ftell(fileptr);         // Get the current byte offset in the file
	rewind(fileptr);
	char send_buffer[10000]; // no link between BUFSIZE and the file size
	int nb = fread(send_buffer, 1, sizeof(send_buffer), fileptr);
	while (!feof(fileptr)) {
		write(new_fd, send_buffer, nb);
		nb = fread(send_buffer, 1, 10000, fileptr);
	}
	write(new_fd, send_buffer, nb);
}

void receive_file_data(int new_fd, char *path) {
	printf("Reading Data\n");
	int size = 10000;
	char p_array[size];
	FILE *fileSent = fopen(path, "wb");
	int nb = read(new_fd, p_array, size);
	while (nb > 0) {
		if (strncmp(p_array, Ok, strlen(Ok)) == 0)
			break;
		fflush(stdout);
		fwrite(p_array, sizeof(char), nb, fileSent);
		nb = read(new_fd, p_array, size);
	}
	fclose(fileSent);
	printf("Finished reading\n");
	fflush(stdout);

	sleep(0.1);
	write(new_fd, Ok, strlen(Ok));
}
