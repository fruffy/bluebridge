/*
 ** client.c -- a stream socket client demo
 */

#include "./lib/538_utils.h"
#include "./lib/debug.h"


/////////////////////////////////// TO DOs ////////////////////////////////////
//	1. IPv6 conversion method
//			| global routing prefix (48) | subnet ID (16) | pointer (64)|
//	2. Check correctness of pointer on server side, it should never segfault.
//			(Ignore illegal operations)
///////////////////////////////////////////////////////////////////////////////
//To add the current correct route
//sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo

struct LinkedPointer {
	struct in6_addr AddrString;
	struct LinkedPointer * Pointer;
};

/*
 * Sends message to allocate memory
 */
struct in6_addr allocateMem(int sockfd, struct addrinfo * p) {
	print_debug("Mallocing send and receive buffers");
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = calloc(BLOCK_SIZE,sizeof(char));
	struct in6_addr * ipv6Pointer = malloc(sizeof(struct in6_addr));
	print_debug("Memcopying ALLOCATE message into send buffer");

	// Send message to server to allocate memory
	memcpy(sendBuffer, ALLOC_CMD, sizeof(ALLOC_CMD));

	sendUDP(sockfd, sendBuffer,BLOCK_SIZE, p);
	print_debug("Waiting to receive replying receive buffer");
	// Wait to receive a message from the server
	int numbytes = receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);
	print_debug("Extracted: %p from server", (void *)(*ipv6Pointer).s6_addr);
	printNBytes((char *)ipv6Pointer->s6_addr,IPV6_SIZE);

	print_debug("Received %d bytes", numbytes);
	print_debug("Parsing response");
	// Parse the response

	struct in6_addr retVal;

	if (memcmp(receiveBuffer,"ACK",3) == 0) {
		print_debug("Response was ACK");
		// If the message is ACK --> successful
		struct in6_addr * remotePointer =  calloc(1,sizeof(struct in6_addr));

		print_debug("Memcopying the pointer");
		printNBytes(receiveBuffer+4,IPV6_SIZE);
		memcpy(remotePointer, receiveBuffer+4, IPV6_SIZE);
		
		//TODO: Remove?
		//char formatted_string[100] = {};
		// sprintf(formatted_string, "Got %" PRIx64 " from server", remotePointer);
		//print_debug("Got %" PRIx64 " from server", remotePointer);
		printNBytes((char *)remotePointer,IPV6_SIZE);

		print_debug("Got: %p from server\n", (void *)remotePointer);
		print_debug("Setting remotePointer to be return value");
		retVal = *remotePointer;
	} else {
		print_debug("Response was not successful");
		// Not successful so we send another message?
		memcpy(sendBuffer, "What's up?", sizeof("What's up?"));
		//sendMsg(sockfd, sendBuffer, sizeof("What's up?"));
		sendUDP(sockfd, sendBuffer,BLOCK_SIZE, p);


		// TODO: why do we return 0? Isn't there a better number 
		// (i.e. -1)

		print_debug("Keeping return value as 0");
	}

	print_debug("Freeing sendBuffer and receiveBuffer memory");
	free(sendBuffer);
	free(receiveBuffer);

	print_debug("Returning value");
	//TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
	return retVal;
}

/*
 * Sends a write command to the sockfd for pointer remotePointer
 */
int writeToMemory(int sockfd, struct addrinfo * p, char * payload,  struct in6_addr * toPointer) {
	print_debug("Mallocing sendBuffer and receiveBuffer");
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;

	print_debug("Copying the data toPointer the send buffer");

	// Create the data
	memcpy(sendBuffer, WRITE_CMD, sizeof(WRITE_CMD));
	size += sizeof(WRITE_CMD) - 1; // Want it to rewrite null terminator
/*	memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);
	size += IPV6_SIZE;*/

	memcpy(sendBuffer+size+1,payload, BLOCK_SIZE - size);
	size += strlen(payload);

	printf("Sending Data: %d bytes to ", size);
	printNBytes((char*) toPointer->s6_addr, IPV6_SIZE);

	print_debug("Sending message");
	sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);


	print_debug("Waiting for response");
	receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);


	print_debug("Freeing sendBuffer and receiveBuffer");
	free(sendBuffer);
	free(receiveBuffer);

	print_debug("Returning");
	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Releases the remote memory
 */
int releaseMemory(int sockfd, struct addrinfo * p,  struct in6_addr * toPointer) {
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;

	// Create message
	memcpy(sendBuffer+size, FREE_CMD, sizeof(FREE_CMD));
	size += sizeof(FREE_CMD) - 1; // Should be 5
	memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);
	size += IPV6_SIZE; // Should be 8, total of 13

	printf("Releasing Data with pointer: ");
	printNBytes((char*)toPointer->s6_addr,IPV6_SIZE);

	// Send message
	//sendMsg(sockfd, sendBuffer, size);
	sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);

	// Receive response
	//receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);

	free(sendBuffer);
	free(receiveBuffer);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Reads the remote memory
 */
char * getMemory(int sockfd, struct addrinfo * p, struct in6_addr * toPointer) {
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;

	// Prep message
	memcpy(sendBuffer, GET_CMD, sizeof(GET_CMD));
	size += sizeof(GET_CMD) - 1; // Should be 4
	memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);
	size += IPV6_SIZE; // Should be 8, total 12

	printf("Retrieving Data with pointer: ");
	printNBytes((char*)toPointer,IPV6_SIZE);

	// Send message
	//sendMsg(sockfd, sendBuffer, size);
	sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	// Receive response
	//receiveMsg(sockfd, receiveBuffer, BLOCK_SIZE);
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);


	free(sendBuffer);

	return receiveBuffer;
}



int basicOperations( int sockfd, struct addrinfo * p) {
	int i;
	// Initialize remotePointers array
	struct LinkedPointer * rootPointer = malloc( sizeof(struct LinkedPointer));
	struct LinkedPointer * nextPointer = rootPointer;
	//init the root element
	nextPointer->Pointer = malloc( sizeof(struct LinkedPointer));
	nextPointer->AddrString = allocateMem(sockfd, p);
	for (i = 0; i < 9; i++) {
		nextPointer = nextPointer->Pointer;
		nextPointer->Pointer = malloc( sizeof(struct LinkedPointer));
		nextPointer->AddrString = allocateMem(sockfd, p);
		//printNBytes((char *) rootPointer->AddrString.s6_addr, 16);
		//printNBytes((char *) nextPointer->AddrString.s6_addr, 16);
	}
	//don't point to garbage
	nextPointer->Pointer = NULL;
	
	i = 1;
	while(rootPointer != NULL)	{

		printf("Iteration %d\n", i);
		struct in6_addr remoteMemory = rootPointer->AddrString;
		printf("Using Pointer: %p\n", (void *) getPointerFromIPv6(rootPointer->AddrString));
		print_debug("Creating payload");
		srand(time(NULL));
		char * payload = get_rdm_string(BLOCK_SIZE,  rand()%100);
		writeToMemory(sockfd, p, payload, &remoteMemory);
		char * test = getMemory(sockfd, p, &remoteMemory);
		printf("Results of memory store: %.50s\n", test);
		releaseMemory(sockfd, p, &remoteMemory);
		free(test);
		free(payload);
		rootPointer = rootPointer->Pointer;
		i++;
	}
}


/*
 * Main workhorse method. Parses arguments, setups connections
 * Allows user to issue commands on the command line.
 */
int main(int argc, char *argv[]) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	
	//specify interactive or automatic client mode
	const int isAutoMode = 1;
	//Socket operator variables
	const int on=1, off=0;

	if (argc < 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "::1";
		argv[2] = "5000";
	}

	//Routing configuration
	// This is a temporary solution to enable the forwarding of unknown ipv6 subnets
	printf("%d\n",system("sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo"));


	// Tells the getaddrinfo to only return sockets
	// which fit these params.
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;

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
		setsockopt(sockfd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
		setsockopt(sockfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
		setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
		break;
	}

	if(isAutoMode) {
		basicOperations(sockfd, p);
	} else {
		//interactiveMode(sockfd, p);
	}



	freeaddrinfo(servinfo); // all done with this structure

	// TODO: send close message so the server exits
	close(sockfd);

	return 0;
}




//TODO: Remove?
struct PointerMap {
	char* AddrString;
	struct in6_addr* Pointer;
};
/*
 * Interactive structure for debugging purposes
 */
int interactiveMode( int sockfd,  struct addrinfo * p) {
	long unsigned int len = 200;
	char input[len];
	char * localData;
	int count = 0;
	int i;
	struct PointerMap remotePointers[100];
	// Initialize remotePointers array
	for (i = 0; i < 100; i++) {
		remotePointers[i].AddrString = (char *) malloc(100);
		remotePointers[i].Pointer = malloc(sizeof(struct in6_addr));
}
	int active = 1;
	while (active) {
		memset(input, 0, len);
		getLine("Please specify if you would like to (L)ist pointers, (A)llocate, (F)ree, (W)rite, or (R)equest data.\nPress Q to quit the program.\n", input, sizeof(input));
		if (strcmp("A", input) == 0) {
			memset(input, 0, len);
			getLine("Specify a number of address or press enter for one.\n", input, sizeof(input));

			if (strcmp("", input) == 0) {
				print_debug("Calling allocateMem");
				
				struct in6_addr remoteMemory = allocateMem(sockfd, p)
				
				// char formatted_string[100] = {};
				// sprintf(formatted_string, "Got %" PRIx64 " from call", remoteMemory);
				//print_debug("Got %" PRIx64 " from call", remoteMemory);
				print_debug("Got %p from call", (void *) remoteMemory.s6_addr);

				print_debug("Creating pointer to remote memory address");

				char addrString[100] = {};
				//sprintf(addrString, "%" PRIx64, remoteMemory);
				memcpy(addrString,&remoteMemory, POINTER_SIZE);
				strcpy(remotePointers[count].AddrString, addrString);
				memcpy(remotePointers[count++].Pointer, &remoteMemory, sizeof(remoteMemory));
				printf("Allocated pointer 0x%s\n", addrString);
				printf("Allocated pointer address %p\n", &remoteMemory);
			} else {
				int num = atoi(input);

				// char message[100] = {};
				// sprintf(message, "Received %d as input", num);
				print_debug("Received %d as input", num);

				int j; 
				for (j = 0; j < num; j++) {
					print_debug("Calling allocateMem");

					struct in6_addr remoteMemory = allocateMem(sockfd, p);
					
					// char formatted_string[100] = {};
					// sprintf(formatted_string, "Got %" PRIx64 " from call", remoteMemory);
					//print_debug("Got %" PRIx64 " from call", remoteMemory);
					print_debug("Got %p from call", (void *) remoteMemory.s6_addr);


					print_debug("Creating pointer to remote memory address");

					char addrString[100] = {};
					//sprintf(addrString, "%" PRIx64, remoteMemory);

					memcpy(addrString,&remoteMemory, POINTER_SIZE);
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
						// char message[100] = {};
						// sprintf(message, "Retrieving data from pointer 0x%s\n", remotePointers[i].AddrString);
						print_debug("Retrieving data from pointer 0x%s", remotePointers[i].AddrString);
						localData = getMemory(sockfd, p, remotePointers[i].Pointer);
						printf("Retrieved Data (first 80 bytes): %.*s\n", 80, localData);
					}
				}
			} else {
				struct in6_addr pointer = getIPv6FromPointer((uint64_t) input);
				// char message[100] = {};
				// sprintf(message, "Retrieving data from pointer 0x%p\n", (void *) pointer);
				print_debug("Retrieving data from pointer 0x%p", (void *) pointer.s6_addr);
				localData = getMemory(sockfd, p, &pointer);
				printf("Retrieved Data (first 80 bytes): %.*s\n", 80, localData);
			}
		} else if (strcmp("W", input) == 0) {
			memset(input, 0, len);
			getLine("Please specify the memory address.\n", input, sizeof(input));

			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (strcmp(remotePointers[i].AddrString, "") != 0) {
						printf("Writing data to pointer 0x%s\n", remotePointers[i].AddrString);
						srand(time(NULL));
						char * payload = get_rdm_string(BLOCK_SIZE,  rand()%100);
						writeToMemory(sockfd, p, payload, remotePointers[i].Pointer);
					}
				}
			} else {
				struct in6_addr pointer = getIPv6FromPointer((uint64_t) input);
				printf("Writing data to pointer %p\n", (void *) pointer.s6_addr);
				srand(time(NULL));
				char * payload = get_rdm_string(BLOCK_SIZE,  rand()%100);
				writeToMemory(sockfd, p, payload, remotePointers[i].Pointer);
			}
		} else if (strcmp("F", input) == 0) {
			memset(input, 0, len);
			getLine("Please specify the memory address.\n", input, sizeof(input));
			
			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (strcmp(remotePointers[i].AddrString, "") != 0) {
						printf("Freeing pointer 0x%s\n", remotePointers[i].AddrString);
						releaseMemory(sockfd, p, remotePointers[i].Pointer);
						remotePointers[i].AddrString = "";
						remotePointers[i].Pointer = 0;
					}
				}
			} else {
				struct in6_addr pointer = getIPv6FromPointer((uint64_t) input);
				printf("Freeing pointer %p\n", (void *) pointer.s6_addr);
				releaseMemory(sockfd, p, &pointer);
				for (i = 0; i < 100; i++) {
					if (strcmp(remotePointers[i].AddrString, input+2) == 0){
						remotePointers[i].AddrString = "";
						remotePointers[i].Pointer = 0;
					}
				}
			}
		//TODO: Remove?
		} else if (strcmp("T", input) == 0) {
			memset(input, 0, len);
			getLine("Please specify the memory address.\n", input, sizeof(input));
			uint64_t pointer = getPointerFromString(input);
			printf("Input pointer: %p\n", (void *) pointer);

			struct in6_addr temp = getIPv6FromPointerStr(pointer);

			printf("Received struct...\n");

			uint64_t newpointer = getPointerFromIPv6Str(temp);

//			printf("New pointer: %" PRIx64 ", Old pointer: %" PRIx64 "\n", newpointer, pointer);
			printf("New pointer: %p, Old pointer: %p\n", (void *) newpointer, (void *) pointer);


		} else if (strcmp("Q", input) == 0) {
			active = 0;
			printf("Ende Gelaende\n");
		} else {
			printf("Try again.\n");
		}
	}
}