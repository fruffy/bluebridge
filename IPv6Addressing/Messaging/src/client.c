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
#include <inttypes.h>

#include "./lib/538_utils.h"

#define BLOCK_SIZE 100 // max number of bytes we can get at once

struct PointerMap {
	char* AddrString;
	uint64_t* Pointer;
};

/*
 * Sends message to allocate memory
 */
uint64_t allocateMem(int sockfd) {
	char sendBuffer[BLOCK_SIZE];
	char receiveBuffer[BLOCK_SIZE];
	char * splitResponse;
	//srand((unsigned int) time(NULL));
	//memcpy(sendBuffer, gen_rdm_bytestream(BLOCK_SIZE), BLOCK_SIZE);

	// Send message to server to allocate memory
	memcpy(sendBuffer, "ALLOCATE", sizeof("ALLOCATE"));
	sendMsg(sockfd, sendBuffer, sizeof("ALLOCATE"));

	// Wait to receive a message from the server
	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	// Parse the response
	splitResponse = strtok(receiveBuffer, ":");

	// printf("%s\n", splitResponse);
	if (strcmp(receiveBuffer,"ACK") == 0) {
		// If the message is ACK --> successful
		uint64_t remotePointer =  0;
		//(uint64_t *) strtok(NULL, ":");
		memcpy(&remotePointer, strtok(NULL, ":"),8);
		// printf("After Pasting\n");
		//memcpy(sendBuffer,"DATASET:",8);
		//memcpy(&zremotePointer,strtok(NULL, ":"),8);
		return remotePointer;
	} else {
		// Not successful so we send another message?
		memcpy(sendBuffer, "What's up?", sizeof("What's up?"));
		sendMsg(sockfd, sendBuffer, sizeof("What's up?"));
		// TODO: why do we return 0? Isn't there a better number 
		// (i.e. -1)
		return 0;
	}
}

/*
 * Sends a write command to the sockfd for pointer remotePointer
 */
int writeToMemory(int sockfd, uint64_t * remotePointer, int index) {
	char sendBuffer[BLOCK_SIZE];
	char receiveBuffer[BLOCK_SIZE];

	char* payload = malloc(20 * sizeof(char));
	sprintf(payload,":MY DATASET %d", index);

	// printf("Payload: %s\n", payload);

	srand((unsigned int) time(NULL));

	int size = 0;

	// Create the data
	//memcpy(sendBuffer, gen_rdm_bytestream(BLOCK_SIZE), BLOCK_SIZE);
	memcpy(sendBuffer, "WRITE:", sizeof("WRITE:"));
	// printf("%lu\n", sizeof("WRITE:"));
	size += 6;
	memcpy(sendBuffer+6,remotePointer,8);
	size += 8;
	memcpy(sendBuffer+14,payload, strlen(payload));
	size += strlen(payload);
	// printf("Size of payload %lu, length %d\n", sizeof(payload), (int) strlen(payload));
	// printf("Size: %d\n", size);

	// Send the data
	printf("Sending Data: %lu bytes as ",sizeof(sendBuffer));
	printBytes((char*) remotePointer);
	// printf("Send buffer: %s", sendBuffer);
	sendMsg(sockfd, sendBuffer, size);

	// Wait for the response
	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	// printf("%s\n",receiveBuffer);
}

/*
 * Releases the remote memory
 */
int releaseMemory(int sockfd, uint64_t * remotePointer) {
	char sendBuffer[BLOCK_SIZE];
	char receiveBuffer[BLOCK_SIZE];

	// Create message
	memcpy(sendBuffer, "FREE:", sizeof("FREE:"));
	memcpy(sendBuffer+5,remotePointer,8);

	printf("Releasing Data with pointer: ");
	printBytes((char*)remotePointer);

	// Send message
	sendMsg(sockfd, sendBuffer, 13);

	// Receive response
	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	// printf("Server Response: %s\n",receiveBuffer);
}

/*
 * Reads the remote memory
 */
char * getMemory(int sockfd, uint64_t * remotePointer) {
	char sendBuffer[BLOCK_SIZE];
	char * receiveBuffer = malloc(4096*1000);

	// Prep message
	memcpy(sendBuffer, "GET:", sizeof("GET:"));
	memcpy(sendBuffer+4,remotePointer,8);

	printf("Retrieving Data with pointer: ");
	printBytes((char*)remotePointer);

	// Send message
	sendMsg(sockfd, sendBuffer, 12);
	// Receive response
	receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	return receiveBuffer;
}

uint64_t getPointerFromString(char* input) {
	uint64_t address;
	sscanf(input, "%" SCNx64, &address);
	// printf("Received address: %" PRIx64 "\n", address);
	uint64_t pointer = (uint64_t *)address;
	return pointer;
}

/*
 * Main workhorse method. Parses arguments, setups connections
 * Allows user to issue commands on the command line.
 */
int main(int argc, char *argv[]) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	srand(time(NULL));

	if (argc < 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "::1";
		argv[2] = "5000";
	}

	// Tells the getaddrinfo to only return sockets
	// which fit these params.
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// TODO: isn't this a separate method in server.c? Can it be moved
	// to the helper?

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
	char * localData;
	int count = 0;
	int i;
	struct PointerMap remotePointers[100];
	// Initialize remotePointers array
	for (i = 0; i < 100; i++) {
		remotePointers[i].AddrString = (char *) malloc(100);
		remotePointers[i].Pointer = malloc(sizeof(uint64_t));
	}

	while (active) {
		memset(input, 0, len);
		getLine("Please specify if you would like to (L)ist pointers, (A)llocate, (F)ree, (W)rite, or (R)equest data.\nPress Q to quit the program.\n", input, sizeof(input));
		if (strcmp("A", input) == 0) {
			memset(input, 0, len);
			getLine("Specify a number of address or press enter for one.\n", input, sizeof(input));

			if (strcmp("", input) == 0) {
				uint64_t remoteMemory = allocateMem(sockfd);
				char addrString[100] = {};
				sprintf(addrString, "%" PRIx64, remoteMemory);
				strcpy(remotePointers[count].AddrString, addrString);
				memcpy(remotePointers[count++].Pointer, &remoteMemory, sizeof(remoteMemory));
				printf("Allocated pointer 0x%s\n", addrString);
				printf("Allocated pointer address %p\n", &remoteMemory);
			} else {
				int num = atoi(input);
				int j; 
				for (j = 0; j < num; j++) {
					uint64_t remoteMemory = allocateMem(sockfd);
					char addrString[100] = {};
					sprintf(addrString, "%" PRIx64, remoteMemory);
					strcpy(remotePointers[count].AddrString, addrString);
					memcpy(remotePointers[count++].Pointer, &remoteMemory, sizeof(remoteMemory));
					printf("Allocated pointer 0x%s\n", addrString);
					printf("Allocated pointer address %p\n", &remoteMemory);
				}
			}
		} else if (strcmp("L", input) == 0){
			printf("Remote Address\tPointer\n");
			for (i = 0; i < 100; i++){
				if (strcmp(remotePointers[i].AddrString, "") != 0) {
					printf("0x%s\t%p\n", remotePointers[i].AddrString, remotePointers[i].Pointer);
				}
			}
		} else if (strcmp("R", input) == 0) {
			memset(input, 0, len);
			getLine("Please specify the memory address.\n", input, sizeof(input));

			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (strcmp(remotePointers[i].AddrString, "") != 0) {
						printf("Retrieving data from pointer 0x%s\n", remotePointers[i].AddrString);
						localData = getMemory(sockfd, remotePointers[i].Pointer);
						printf("Retrieved Data: %s\n",localData);
					}
				}
			} else {
				uint64_t pointer = getPointerFromString(input);
				printf("Retrieving data from pointer %p\n", (void *) pointer);
				localData = getMemory(sockfd, &pointer);
				printf("Retrieved Data: %s\n",localData);
			}
		} else if (strcmp("W", input) == 0) {
			memset(input, 0, len);
			getLine("Please specify the memory address.\n", input, sizeof(input));

			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (strcmp(remotePointers[i].AddrString, "") != 0) {
						printf("Writing data to pointer 0x%s\n", remotePointers[i].AddrString);
						// TODO change to write different string
						writeToMemory(sockfd, remotePointers[i].Pointer, i);
					}
				}
			} else {
				uint64_t pointer = getPointerFromString(input);
				printf("Writing data to pointer %p\n", (void *) pointer);
				writeToMemory(sockfd, &pointer, rand());
			}
		} else if (strcmp("F", input) == 0) {
			memset(input, 0, len);
			getLine("Please specify the memory address.\n", input, sizeof(input));
			
			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (strcmp(remotePointers[i].AddrString, "") != 0) {
						printf("Freeing pointer 0x%s\n", remotePointers[i].AddrString);
						releaseMemory(sockfd, remotePointers[i].Pointer);
						remotePointers[i].AddrString = "";
						remotePointers[i].Pointer = 0;
					}
				}
			} else {
				uint64_t pointer = getPointerFromString(input);
				printf("Freeing pointer %p\n", (void *) pointer);
				releaseMemory(sockfd, &pointer);
				for (i = 0; i < 100; i++) {
					if (strcmp(remotePointers[i].AddrString, input+2) == 0){
						// printf("Match!\n");
						remotePointers[i].AddrString = "";
						remotePointers[i].Pointer = 0;
					}
				}
			}
		} else if (strcmp("Q", input) == 0) {
			active = 0;
			printf("Ende Gelaende\n");
		} else {
			printf("Try again.\n");
		}
	}
	freeaddrinfo(servinfo); // all done with this structure

	// TODO: send close message so the server exits
	close(sockfd);

	return 0;
}
