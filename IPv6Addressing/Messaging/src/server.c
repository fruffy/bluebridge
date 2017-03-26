/*
 ** server.c -- a stream socket server demo
 */

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
#include <errno.h>
#include "./lib/538_utils.h"

#define BACKLOG 10     // how many pending connections queue will hold

char *varadr_char[1000];
int countchar = 0;

/*
 * Frees global memory
 */
int cleanMemory() {
	int i;
	for (i = 0; i <= countchar; i++) {
		free(varadr_char[i]);
	}

	return EXIT_SUCCESS;
}

/*
 * Adds a character to the global memory
 */
int addchar(char* charadr) {
	if (charadr == NULL ) {
		perror("\nError when trying to allocate a pointer! \n");
		cleanMemory();
		exit(EXIT_FAILURE);
	}

	varadr_char[countchar] = charadr;
	countchar++;
	return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Creates a new pointer to a new part of memory then sends it?
 */
int providePointer (int new_fd, char * receiveBuffer) {
	print_debug("Creating allocated and sendBuffer with malloc");
	char * allocated = (char*) malloc(4096*1000);
	char * sendBuffer = malloc(BLOCK_SIZE*sizeof(char));
	int size = 0;

	print_debug("Constructing message in sendBuffer");
	memcpy(sendBuffer+size, "ACK:", sizeof("ACK:"));
	size += sizeof("ACK:") - 1;
	memcpy(sendBuffer+size, &allocated, sizeof(allocated));
	size += sizeof(allocated);

	char message[100] = {};
	sprintf(message, "End sendBuffer size: %d", size);
	print_debug(message);

	//printBytes(sendBuffer);
	//printf("Interpretation of Server %s \n", sendBuffer);
	print_debug("Sending message");
	sendMsg(new_fd, sendBuffer, size);
	printf("\nAllocated Pointer: %p -> %s\n",allocated, allocated);
	printf("Content %s is stored at %p!\n\n", allocated, (void*)allocated);

	print_debug("Freeing sendBuffer");
	free(sendBuffer);

	print_debug("Returning dummy value");
	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int writeMem (int new_fd, char * receiveBuffer) {
	char* sendBuffer = malloc(BLOCK_SIZE*sizeof(char));
	char * target;

	char message1[100] = {};
	sprintf(message1, "Receive buffer: %s\n", receiveBuffer);
	print_debug(message1);

	printBytes(receiveBuffer);

	// TODO: why is this +9?
	char * dataToWrite = receiveBuffer + 9;
	
	char * message2 = malloc(BLOCK_SIZE *sizeof(char));
	sprintf(message2, "Data received: %s\n", dataToWrite);
	//print_debug(message2);
	
	// Copy the first eight bytes of receive buffer into the target
	memcpy(&target, receiveBuffer, 8);

	char message3[100] = {};
	sprintf(message3, "Target pointer: %p", (void*) target);
	print_debug(message3);

	//printf("Target Pointer: %p -> %s\n",target, target);

	printf("Length: %lu\n", strlen(dataToWrite));

	memcpy(target, dataToWrite, strlen(dataToWrite));


	printf("Content %s is stored at %p!\n", target, (void*)target);
	memcpy(sendBuffer, "ACK", sizeof("ACK"));
	// Send the sendBuffer to the new fd (socket), only send 3 bytes.
	// TODO: shouldn't this be sizeof("ACK")?
	sendMsg(new_fd,sendBuffer,3);

	free(sendBuffer);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * TODO: explain.
 * This is freeing target memory?
 */
int freeMem (int new_fd, char * receiveBuffer) {
	char* sendBuffer = malloc(BLOCK_SIZE*sizeof(char));
	char * target;

	// Why copy when you're freeing the memory?
	memcpy(&target, receiveBuffer, 8);

	printf("Content stored at %p has been freed!\n", (void*)target);
	free(target);
	memcpy(sendBuffer, "ACK", sizeof("ACK"));
	// Send the sendBuffer to the new fd (socket), only send 3 bytes.
	// TODO: shouldn't this be sizeof("ACK")?
	sendMsg(new_fd,sendBuffer,3);

	free(sendBuffer);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Gets memory and sends it
 */
int getMem (int new_fd, char * receiveBuffer) {
	char* sendBuffer = malloc(BLOCK_SIZE*sizeof(char));
	char * target;

	// Copy eight bytes of the receiveBuffer into the target
	memcpy(&target, receiveBuffer, 8);

	printf("Content %s is currently stored at %p!\n", target, (void*)target);


	printf("Content %s will be delivered to client!\n", target);
	// Copy the target into the send buffer (entire BLOCK_SIZE)
	// TODO: why do this if target only has 8 bytes?
	memcpy(sendBuffer, target, BLOCK_SIZE);

	// Send the sendBuffer (entire BLOCK_SIZE) to new_fd
	sendMsg(new_fd,sendBuffer,BLOCK_SIZE);

	free(sendBuffer);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Request handler for socket new_fd
 * TODO: get message format
 */
void handleClientRequests(int new_fd) {
	char receiveBuffer[BLOCK_SIZE];
	int numbytes;
	while (1) {
		printf("Waiting for client message...\n");

		// Get the message
		numbytes = receiveMsg(new_fd, receiveBuffer, BLOCK_SIZE);

		// Parse the message (delimited by :)
		printf("Message from client: %i bytes\n", numbytes);
		char * dataToWrite = strtok(receiveBuffer, ":");
		printf("Client Command: %s\n", dataToWrite);
		char * splitResponse = strtok(NULL, ":");

		// Switch on the client command
		if (strcmp(dataToWrite, "ALLOCATE") == 0) {
			providePointer(new_fd, splitResponse);
		} else if (strcmp(dataToWrite, "WRITE") == 0) {
			printf("Writing to pointer: ");
			printBytes(splitResponse);
			writeMem(new_fd, splitResponse);
		} else if (strcmp(dataToWrite, "FREE") == 0) {
			printf("Deleting pointer: ");
			printBytes(splitResponse);
			freeMem(new_fd, splitResponse);
		} else if (strcmp(dataToWrite, "GET") == 0) {
			printf("Retrieving data: ");
			printBytes(splitResponse);
			getMem(new_fd, splitResponse);
		} else {
			// TODO: what is this doing?
			if (send(new_fd, "Hello, world!", 13, 0) == -1) {
				perror("ERROR writing to socket");
				exit(1);
			}
		}
	}
}

/*
 * TODO: explain
 * Binds to the next available address?
 */
struct addrinfo* bindSocket(struct addrinfo* p, struct addrinfo* servinfo,
		int* sockfd) {
	int yes = 1;

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
				== -1) {
			perror("setsockopt");
			exit(1);
		}
		if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(*sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}
	return p;
}

/*
 * Accept a connection on sockfd
 */
int acceptConnections(int sockfd) {
	// connector's address information
	struct sockaddr_storage their_addr; 
	char s[INET6_ADDRSTRLEN];
	socklen_t sin_size = sizeof their_addr;
	//wait for incoming connection
	int temp_fd = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size);
	inet_ntop((&their_addr)->ss_family,
			get_in_addr((struct sockaddr*) &their_addr), s, sizeof s);

	printf("server: got connection from %s\n", s);
	return temp_fd;
}

/*
 * Main workhorse method. Parses command args and does setup.
 * Blocks waiting for connections.
 */
int main(int argc, char *argv[]) {
	int sockfd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sigaction sa;
	int rv;

	// hints = specifies criteria for selecting the socket address
	// structures
	memset(&hints, 0, sizeof(hints));
	if (argc < 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "5000";
	}
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	p = bindSocket(p, servinfo, &sockfd);

	// Frees the memory allocated for servinfo (list of possible sockets)
	freeaddrinfo(servinfo);
	// TODO: should free hints as well?

	// Check to make sure the server was able to bind
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	// Listen on the socket with a queue size of BACKLOG
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	// TODO: explain. 
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	// Start waiting for connections
	printf("server: waiting for connections...\n");
	while (1) {
		int new_fd = acceptConnections(sockfd);
		if (new_fd == -1) {
			perror("accept");
			exit(1);
		}
		if (!fork()) {
			// fork the process
			close(sockfd); // child doesn't need the listener
			handleClientRequests(new_fd);
		}

		close(new_fd); // parent doesn't need this
	}

	return 0;
}
