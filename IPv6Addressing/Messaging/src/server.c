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
#define BLOCK_SIZE 100 // max number of bytes we can get at once

char *varadr_char[1000];
int countchar = 0;




int cleanMemory() {
	int i;
	for (i = 0; i <= countchar; i++) {
		free(varadr_char[i]);
	}

	return EXIT_SUCCESS;
}

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


int providePointer (int new_fd, char * receiveBuffer) {
	char * allocated = (char*) malloc(4096*1000);
	char sendBuffer[BLOCK_SIZE];

	memcpy(sendBuffer,"ACK:",4);
	memcpy(sendBuffer+4,&allocated,8);

	printBytes(sendBuffer);
	printf("Interpretation of Server %s \n", sendBuffer);

	sendMsg(new_fd, sendBuffer,12);
	printf("\nAllocated Pointer: %p -> %s\n",allocated, allocated);
	printf("Content %s is stored at %p!\n", allocated, (void*)allocated);
}

int writeMem (int new_fd, char * receiveBuffer) {
	char sendBuffer[BLOCK_SIZE];
	char * target;

	char * dataToWrite = receiveBuffer +9;
	printf("Third Split: %s\n", dataToWrite);
	memcpy(&target, receiveBuffer, 8);

	printf("Target Pointer: %p -> %s\n",target, target);

	memcpy(target, dataToWrite, BLOCK_SIZE);


	printf("Content %s is stored at %p!\n", target, (void*)target);
	memcpy(sendBuffer, "ACK", sizeof("ACK"));
	sendMsg(new_fd,sendBuffer,3);

}
int freeMem (int new_fd, char * receiveBuffer) {
	char sendBuffer[BLOCK_SIZE];
	char * target;

	memcpy(&target, receiveBuffer, 8);

	printf("Content stored at %p has been freed!\n", (void*)target);
	free(target);
	memcpy(sendBuffer, "ACK", sizeof("ACK"));
	sendMsg(new_fd,sendBuffer,3);

}

int getMem (int new_fd, char * receiveBuffer) {
	char sendBuffer[BLOCK_SIZE];
	char * target;

	memcpy(&target, receiveBuffer, 8);

	printf("Content %s is currently stored at %p!\n", target, (void*)target);


	printf("Content %s will be delivered to client!\n", target);
	memcpy(sendBuffer, target, BLOCK_SIZE);
	sendMsg(new_fd,sendBuffer,BLOCK_SIZE);
}


void handleClientRequests(int new_fd) {
	char receiveBuffer[BLOCK_SIZE];
	int numbytes;
	while (1) {
		printf("Waiting for client message...\n");

		numbytes = receiveMsg(new_fd, receiveBuffer, BLOCK_SIZE);
		printf("Message from client: %i bytes\n", numbytes);
		char * dataToWrite = strtok(receiveBuffer, ":");
		printf("Client Command: %s\n", dataToWrite);
		char * splitResponse = strtok(NULL, ":");

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
			if (send(new_fd, "Hello, world!", 13, 0) == -1) {
				perror("ERROR writing to socket");
				exit(1);
			}
		}
	}
}

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

int acceptConnections(int sockfd) {
	struct sockaddr_storage their_addr; // connector's address information
	char s[INET6_ADDRSTRLEN];
	socklen_t sin_size = sizeof their_addr;
	//wait for incoming connection
	int temp_fd = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size);
	inet_ntop((&their_addr)->ss_family,
			get_in_addr((struct sockaddr*) &their_addr), s, sizeof s);

	printf("server: got connection from %s\n", s);
	return temp_fd;
}

int main(int argc, char *argv[]) {
	int sockfd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sigaction sa;
	int rv;

	memset(&hints, 0, sizeof hints);
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
