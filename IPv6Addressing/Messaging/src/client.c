/*
 ** client.c -- a stream socket client demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>

#include "./lib/538_utils.h"

#define BLOCK_SIZE 100 // max number of bytes we can get at once

uint64_t allocateMem(int sockfd) {
	char sendBuffer[BLOCK_SIZE];
	char receiveBuffer[BLOCK_SIZE];
	char * splitResponse;
	//srand((unsigned int) time(NULL));
	//memcpy(sendBuffer, gen_rdm_bytestream(BLOCK_SIZE), BLOCK_SIZE);
	memcpy(sendBuffer, "ALLOCATE", sizeof("ALLOCATE"));
	sendMsg(sockfd, sendBuffer, sizeof("ALLOCATE"));



	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	splitResponse = strtok(receiveBuffer, ":");

	printf("%s\n", splitResponse);
	if (strcmp(receiveBuffer,"ACK") == 0) {
		uint64_t remotePointer =  0;
		//(uint64_t *) strtok(NULL, ":");
		memcpy(&remotePointer, strtok(NULL, ":"),8);
		printf("After Pasting\n");
		//memcpy(sendBuffer,"DATASET:",8);
		//memcpy(&zremotePointer,strtok(NULL, ":"),8);
		return remotePointer;
	} else {
		memcpy(sendBuffer, "What's up?", sizeof("What's up?"));
		sendMsg(sockfd, sendBuffer, sizeof("What's up?"));
		return 0;
	}
}

int writeToMemory(int sockfd, uint64_t * remotePointer) {
	char sendBuffer[BLOCK_SIZE];
	char receiveBuffer[BLOCK_SIZE];

	srand((unsigned int) time(NULL));
	//memcpy(sendBuffer, gen_rdm_bytestream(BLOCK_SIZE), BLOCK_SIZE);
	memcpy(sendBuffer, "WRITE:", sizeof("WRITE:"));
	memcpy(sendBuffer+6,remotePointer,8);
	memcpy(sendBuffer+14,":MY DATASET", sizeof(":MY DATASET"));

	printf("Sending Data: %lu bytes as ",sizeof(sendBuffer));
	printBytes((char*) remotePointer);
	sendMsg(sockfd, sendBuffer, 25);
	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	printf("%s\n",receiveBuffer);
}
int releaseMemory(int sockfd, uint64_t * remotePointer) {
	char sendBuffer[BLOCK_SIZE];
	char receiveBuffer[BLOCK_SIZE];

	memcpy(sendBuffer, "FREE:", sizeof("FREE:"));
	memcpy(sendBuffer+5,remotePointer,8);

	printf("Releasing Data with pointer: ");
	printBytes((char*)remotePointer);

	sendMsg(sockfd, sendBuffer, 13);
	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	printf("Server Response: %s\n",receiveBuffer);
}
char * getMemory(int sockfd, uint64_t * remotePointer) {
	char sendBuffer[BLOCK_SIZE];
	char * receiveBuffer = malloc(4096*1000);

	memcpy(sendBuffer, "GET:", sizeof("GET:"));
	memcpy(sendBuffer+4,remotePointer,8);

	printf("Retrieving Data with pointer: ");
	printBytes((char*)remotePointer);

	sendMsg(sockfd, sendBuffer, 12);
	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	return receiveBuffer;
}

int main(int argc, char *argv[]) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc < 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "::1";
		argv[2] = "5000";
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
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

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr), s,
			sizeof s);
	printf("client: connecting to %s\n", s);

	int active = 1;
	long unsigned int len = 200;
	char input[len];
	char * localData ;
	while (active) {
		memset(input, 0, len);
		getLine("Please specify if you would like to (A)llocate, (W)rite, or (R)equest data.\nPress Q to quit the program.\n", input, sizeof(input));
		if (strcmp("A", input) == 0) {
			uint64_t remoteMemory = allocateMem(sockfd);
			writeToMemory(sockfd, &remoteMemory);
			localData = getMemory(sockfd, &remoteMemory);
			printf("Retrieve Data: %s\n",localData);
			releaseMemory(sockfd, &remoteMemory);
		} else if (strcmp("R", input) == 0) {

		} else if (strcmp("Q", input) == 0) {
			active = 0;
			printf("Ende Gelaende\n");
		} else {
			printf("Try again.\n");
		}
	}
	freeaddrinfo(servinfo); // all done with this structure
	close(sockfd);

	return 0;
}
