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

#include "538_utils.h"

#define BLOCK_SIZE 100 // max number of bytes we can get at once

int main(int argc, char *argv[]) {
	int sockfd, numbytes;
	char receiveBuffer[BLOCK_SIZE];
	char sendBuffer[BLOCK_SIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv, n;
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
	char * splitResponse;
	while (active) {
		memset(input, 0, len);
		int response =	getLine("Please specify if you would like to (S)end or (R)eceive data.\nPress Q to quit the program.\n", input, sizeof(input));
		if (strcmp("S", input) == 0) {
			
			//srand((unsigned int) time(NULL));
			//memcpy(sendBuffer, gen_rdm_bytestream(BLOCK_SIZE), BLOCK_SIZE);
			memcpy(sendBuffer, "WRITE REQUEST", sizeof("WRITE REQUEST"));
			sendMsg(sockfd, sendBuffer, sizeof("WRITE REQUEST"));



			numbytes = receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
			splitResponse = strtok(receiveBuffer, ":");
			printf("Server Response: ");
			printf("First Split: %s\n", splitResponse);
			splitResponse = strtok(NULL, ":");
			printf("Second Split: %s\n", splitResponse);

			if (strcmp(receiveBuffer,"ACK") == 0) {
/*				memcpy(sendBuffer,"WRITE COMMAND", sizeof("WRITE COMMAND"));
				sendMsg(sockfd, sendBuffer, sizeof("WRITE COMMAND"));
				printBytes(numbytes, splitResponse);*/
				numbytes = sprintf(sendBuffer, "DATASET:%s", splitResponse); // puts string into buffer
				printf("Sending Data: %i bytes ",numbytes);
				printf("as %s\n",sendBuffer);
				sendMsg(sockfd, sendBuffer, numbytes);

			} else {
				memcpy(sendBuffer, "What's up?", sizeof("What's up?"));
				sendMsg(sockfd, sendBuffer, sizeof("What's up?"));
			}

			numbytes = receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
			//receiveBuffer[numbytes] = '\0';

			char * availableSpace = receiveBuffer;
			printf("Message from server: %i bytes\n",numbytes);
			printBytes(numbytes,receiveBuffer);

			memcpy(sendBuffer, receiveBuffer, sizeof(receiveBuffer));
			sendMsg(sockfd, sendBuffer, numbytes);

		} else if (strcmp("R", input) == 0) {
			memcpy(sendBuffer, "READ REQUEST", sizeof("READ_REQUEST"));
			//Sockets Layer Call: send()
			n = send(sockfd, sendBuffer, strlen(sendBuffer) + 1, 0);
			if (n < 0)
				perror("ERROR writing to socket");
			if ((numbytes = recv(sockfd, receiveBuffer, BLOCK_SIZE - 1, 0)) == -1) {
				perror("recv");
				exit(1);
			}
			printf("client: received '%02X'\n", (unsigned) *receiveBuffer);
		} else if (strcmp("Q", input) == 0) {
			active = 0;
			printf("Ende Gelaende\n");
		} else {
			printf("Try again.\n");
		}
		memset(sendBuffer, 0, BLOCK_SIZE);
		memset(receiveBuffer, 0, BLOCK_SIZE);
	}
	freeaddrinfo(servinfo); // all done with this structure
	close(sockfd);

	return 0;
}
