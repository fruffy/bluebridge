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


#define BLOCK_SIZE 100 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}
//http://stackoverflow.com/questions/4023895/how-to-read-string-entered-by-user-in-c
static int getLine(char *prmpt, char *buff, size_t sz) {
	int ch, extra;

	// Get line with buffer overrun protection.
	if (prmpt != NULL) {
		printf("%s", prmpt);
		fflush(stdout);
	}
	if (fgets(buff, sz, stdin) == NULL)
		return 0;

	// If it was too long, there'll be no newline. In that case, we flush
	// to end of line so that excess doesn't affect the next call.
	if (buff[strlen(buff) - 1] != '\n') {
		extra = 0;
		while (((ch = getchar()) != '\n') && (ch != EOF))
			extra = 1;
		return (extra == 1) ? 0 : 1;
	}

	// Otherwise remove newline and give string back to caller.
	buff[strlen(buff) - 1] = '\0';
	return 1;
}

unsigned char *gen_rdm_bytestream(size_t num_bytes) {
	unsigned char *stream = malloc(num_bytes);
	size_t i;

	for (i = 0; i < num_bytes; i++) {
		stream[i] = rand();
	}

	return stream;
}

int main(int argc, char *argv[]) {
	int sockfd, numbytes;
	char receiveBuffer[BLOCK_SIZE];
	char sendBuffer[BLOCK_SIZE];
	long long int mem_pointer;
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

	while (active) {
		memset(input, 0, len);
		int response =
				getLine("Please specify if you would like to (S)end or (R)eceive data.\nPress Q to quit the program.\n",
						input, sizeof(input));
		if (strcmp("S", input) == 0) {
			srand((unsigned int) time(NULL));
			memcpy(sendBuffer, gen_rdm_bytestream(BLOCK_SIZE), BLOCK_SIZE);
			//Sockets Layer Call: send()
			memcpy(sendBuffer, "WRITE REQUEST", BLOCK_SIZE);

			n = send(sockfd, sendBuffer, strlen(sendBuffer) + 1, 0);
			if (n < 0)
				perror("ERROR writing to socket");

			memset(sendBuffer, 0, BLOCK_SIZE);
			memset(receiveBuffer, 0, BLOCK_SIZE);

			//Sockets Layer Call: recv()
			if ((numbytes = recv(sockfd, receiveBuffer, BLOCK_SIZE - 1, 0)) == -1) {
				perror("ERROR reading from socket");
				exit(1);
			}
			printf("Pointer from server: ");
			printf("%s\n", receiveBuffer);

			if (strcmp(receiveBuffer,"ACK") == 0) {
				memcpy(sendBuffer, "WRITE COMMAND", BLOCK_SIZE);
				n = send(sockfd, sendBuffer, strlen(sendBuffer) + 1, 0);
					if (n < 0)
						perror("ERROR writing to socket");
			} else {
				memcpy(sendBuffer, "What's up?", BLOCK_SIZE);
				n = send(sockfd, sendBuffer, strlen(sendBuffer) + 1, 0);
				if (n < 0)
					perror("ERROR writing to socket");
			}
			memset(sendBuffer, 0, BLOCK_SIZE);
			memset(receiveBuffer, 0, BLOCK_SIZE);
			//Sockets Layer Call: recv()
			if ((numbytes = recv(sockfd, receiveBuffer, BLOCK_SIZE - 1, 0)) == -1) {
				perror("ERROR reading from socket");
				exit(1);
			}
			int i = 0;
			for (i =numbytes; i >= 0; i--) {
				printf("%02X", (unsigned char) receiveBuffer[i]);
			}
			printf("Message from server: %i bytes\n",numbytes);
			printf("Interpret as:'%02X'\n", (unsigned) receiveBuffer);

		} else if (strcmp("R", input) == 0) {
			memcpy(sendBuffer, "READ REQUEST", BLOCK_SIZE);
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
		fflush(stdin);
	}
	freeaddrinfo(servinfo); // all done with this structure
	close(sockfd);

	return 0;
}
