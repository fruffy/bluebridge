/*
 ** client.c -- a stream socket client demo
 */

#include "./lib/538_utils.h"
#include "./lib/debug.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/////////////////////////////////// TO DOs ////////////////////////////////////
//	1. Check correctness of pointer on server side, it should never segfault.
//		(Ignore illegal operations)
//		-> Maintain list of allocated points
//		-> Should be very efficient
//		-> Judy array insert and delete or hashtable?
//	2. Implement userfaultd on the client side
//	3. Integrate functional ndp proxy server into the server program
//	4. Implement IP subnet state awareness
//		(server allocates memory address related to its assignment)
//	5. Remove unneeded code and print statements
//		Move all buffers to stack instead of heap.
//	6. Fix interactive mode and usability bugs
//	7. Switch to raw socket packets (hope is to get rid of NDP requests)
//	http://stackoverflow.com/questions/15702601/kernel-bypass-for-udp-and-tcp-on-linux-what-does-it-involve
//	https://austinmarton.wordpress.com/2011/09/14/sending-raw-ethernet-packets-from-a-specific-interface-in-c-on-linux/
//	8. Integrate Mihir's asynchronous code and use raw linux threading:
//		http://nullprogram.com/blog/2015/05/15/
//	9. Test INADDR_ANY to see if socket will accept any incoming destination IP address
///////////////////////////////////////////////////////////////////////////////
//To add the current correct route
//sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo
//ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dest=0:0:01ff:0:ffff:ffff:0:0,actions=output:2
struct LinkedPointer {
	struct in6_addr AddrString;
	struct LinkedPointer * Pointer;
};

/*
 * Sends message to allocate memory
 */
struct in6_addr allocateMem(int sockfd, struct addrinfo * p) {
	print_debug("Mallocing send and receive buffers");
	char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	print_debug("Memcopying ALLOCATE message into send buffer");

	// Lines are for ndpproxy DO NOT REMOVE
	struct in6_addr * ipv6Pointer = gen_rdm_IPv6Target();
	memcpy(&(((struct sockaddr_in6*) p->ai_addr)->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
	p->ai_addrlen = sizeof(*ipv6Pointer);
	
	memcpy(sendBuffer, ALLOC_CMD, sizeof(ALLOC_CMD));

	sendUDP(sockfd, sendBuffer,BLOCK_SIZE, p);
	// Wait to receive a message from the server
	int numbytes = receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);
	// print_debug("Extracted: %p from server", (void *)(*ipv6Pointer).s6_addr); // DO NOT REMOVE NEEDED FOR NDPPROXY
	//printNBytes((char *)ipv6Pointer->s6_addr,IPV6_SIZE);
	print_debug("Received %d bytes", numbytes);
	// Parse the response

	struct in6_addr retVal;

	if (memcmp(receiveBuffer,"ACK",3) == 0) {
		print_debug("Response was ACK");
		// If the message is ACK --> successful
		struct in6_addr * remotePointer = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
		
		if (DEBUG) {
			printf("Received pointer data: ");
			printNBytes(receiveBuffer+4,IPV6_SIZE);
		}
		// Copy the returned pointer
		memcpy(remotePointer, receiveBuffer+4, IPV6_SIZE);
		// Insert information about the source host (black magic)
		//00 00 00 00 01 02 00 00 00 00 00 00 00 00 00 00
		//			  ^  ^ these two bytes are stored (subnet and host ID)
		memcpy(remotePointer->s6_addr+4, get_in_addr(p->ai_addr)+4, 2);
		print_debug("Got: %p from server", (void *)remotePointer);

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
	
	//TODO: Error handling
	char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;

	print_debug("Copying the data toPointer the send buffer");

	// Create the data
	memcpy(sendBuffer, WRITE_CMD, sizeof(WRITE_CMD));
	size += sizeof(WRITE_CMD) - 1; // Want it to rewrite null terminator
	memcpy(sendBuffer+size+1,payload, BLOCK_SIZE - size);
	size += strlen(payload);

	if (DEBUG) {
		printf("Sending Data: %d bytes", size);
		printNBytes((char*) toPointer->s6_addr, IPV6_SIZE);
	}

	sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);
	free(sendBuffer);
	free(receiveBuffer);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Releases the remote memory
 */
int releaseMemory(int sockfd, struct addrinfo * p,  struct in6_addr * toPointer) {
	char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;

	// Create message
	memcpy(sendBuffer+size, FREE_CMD, sizeof(FREE_CMD));
	size += sizeof(FREE_CMD) - 1; // Should be 5
	memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);
	size += IPV6_SIZE; // Should be 8, total of 13

	if (DEBUG) {
		printf("Releasing Data with pointer: ");
		printNBytes((char*)toPointer->s6_addr,IPV6_SIZE);
	}

	// Send message
	sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);

	// Receive response
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
	char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;

	// Prep message
	memcpy(sendBuffer, GET_CMD, sizeof(GET_CMD));
	size += sizeof(GET_CMD) - 1; // Should be 4
	memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);
	size += IPV6_SIZE; // Should be 8, total 12

	if (DEBUG) {
		printf("Retrieving Data with pointer: ");
		printNBytes((char*)toPointer,IPV6_SIZE);
	}

	// Send message
	sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	// Receive response
	print_debug("Now waiting")
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);

	free(sendBuffer);

	return receiveBuffer;
}

/*
 * Reads the remote memory
 */
int migrate(int sockfd, struct addrinfo * p, struct in6_addr * toPointer, int machineID) {
	char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer;
	// Allocates storage
	char * ovs_cmd = (char*)malloc(100 * sizeof(char));
	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family,(struct sockaddr *) toPointer->s6_addr, s, sizeof s);


	printf("Getting pointer\n");
	receiveBuffer = getMemory(sockfd, p, toPointer);
	printf("Freeing pointer\n");

	releaseMemory(sockfd, p, toPointer);
	printf("Writing pointer\n");
	sprintf(ovs_cmd, "ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dst=%s,priority=65535,actions=output:%d", s, machineID);
	int status = system(ovs_cmd);
	printf("%d\t%s\n", status, ovs_cmd);

	writeToMemory(sockfd, p, receiveBuffer, toPointer);
	free(sendBuffer);
	free(ovs_cmd);
	return 0;
}

void print_times( uint64_t* alloc_latency, uint64_t* read_latency, uint64_t* write_latency, uint64_t* free_latency, int num_iters){
	FILE *allocF = fopen("alloc_latency.csv", "w");
    if (allocF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *readF = fopen("read_latency.csv", "w");
    if (readF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *writeF = fopen("write_latency.csv", "w");
    if (writeF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *freeF = fopen("free_latency.csv", "w");
    if (freeF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    fprintf(allocF, "latency (ns)\n");
    fprintf(readF, "latency (ns)\n");
    fprintf(writeF, "latency (ns)\n");

    int i;
    for (i = 0; i < num_iters; i++) {
        fprintf(allocF, "%llu\n", (unsigned long long) alloc_latency[i]);
        fprintf(readF, "%llu\n", (unsigned long long) read_latency[i]);
        fprintf(writeF, "%llu\n", (unsigned long long) write_latency[i]);
        fprintf(freeF, "%llu\n", (unsigned long long) free_latency[i]);
    }

    fclose(allocF);
    fclose(readF);
    fclose(writeF);
    fclose(freeF);
}

int basicOperations( int sockfd, struct addrinfo * p) {
	int num_iters = 10;
	uint64_t *alloc_latency = malloc(sizeof(uint64_t) * num_iters);
    assert(alloc_latency);
    memset(alloc_latency, 0, sizeof(uint64_t) * num_iters);

    uint64_t *read_latency = malloc(sizeof(uint64_t) * num_iters);
    assert(read_latency);
    memset(read_latency, 0, sizeof(uint64_t) * num_iters);

    uint64_t *write_latency = malloc(sizeof(uint64_t) * num_iters);
    assert(write_latency);
    memset(write_latency, 0, sizeof(uint64_t) * num_iters);

    uint64_t *free_latency = malloc(sizeof(uint64_t) * num_iters);
    assert(free_latency);
    memset(free_latency, 0, sizeof(uint64_t) * num_iters);
	int i;
	// Initialize remotePointers array
	struct LinkedPointer * rootPointer = (struct LinkedPointer *) malloc( sizeof(struct LinkedPointer));
	struct LinkedPointer * nextPointer = rootPointer;
	//init the root element
	nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
	nextPointer->AddrString = allocateMem(sockfd, p);
	srand(time(NULL));
	for (i = 0; i < num_iters; i++) {
		nextPointer = nextPointer->Pointer;
		nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));

		uint64_t start = getns();
		nextPointer->AddrString = allocateMem(sockfd, p);
		alloc_latency[i] = getns() - start;
		//printNBytes((char *) rootPointer->AddrString.s6_addr, 16);
		//printNBytes((char *) nextPointer->AddrString.s6_addr, 16);
	}
	// don't point to garbage
	// temp fix
	nextPointer->Pointer = NULL;
	
	i = 1;
	srand(time(NULL));
	while(rootPointer != NULL)	{

		printf("Iteration %d\n", i);
		struct in6_addr remoteMemory = rootPointer->AddrString;
		print_debug("Using Pointer: %p\n", (void *) getPointerFromIPv6(rootPointer->AddrString));
		print_debug("Creating payload");
		char * payload = gen_rdm_bytestream(BLOCK_SIZE);

		uint64_t wStart = getns();
		writeToMemory(sockfd, p, payload, &remoteMemory);
		write_latency[i - 1] = getns() - wStart;

		uint64_t rStart = getns();
		char * test = getMemory(sockfd, p, &remoteMemory);
		read_latency[i - 1] = getns() - rStart;

		print_debug("Results of memory store: %.50s\n", test);
		
		uint64_t fStart = getns();
		releaseMemory(sockfd, p, &remoteMemory);
		free_latency[i-1] = getns() - fStart;

		free(test);
		free(payload);
		rootPointer = rootPointer->Pointer;
		i++;
	}

	print_times(alloc_latency, read_latency, write_latency, free_latency, num_iters);

	free(alloc_latency);
	free(write_latency);
	free(read_latency);
	free(free_latency);
}
//TODO: Remove?
struct PointerMap {
	struct in6_addr Pointer;
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
	struct in6_addr remotePointers[100];
	char * lazyZero = calloc(IPV6_SIZE, sizeof(char));
	char s[INET6_ADDRSTRLEN];
	
	// Initialize remotePointers array
	for (i = 0; i < 100; i++) {
		memset(remotePointers[i].s6_addr,0,IPV6_SIZE);
	}
	
	int active = 1;
	while (active) {
		srand(time(NULL));
		memset(input, 0, len);
		getLine("Please specify if you would like to (L)ist, (A)llocate, (F)ree, (W)rite, or (R)equest data.\nPress Q to quit the program.\n", input, sizeof(input));
		if (strcmp("A", input) == 0) {
			memset(input, 0, len);
			getLine("Specify a number of address or press enter for one.\n", input, sizeof(input));

			if (strcmp("", input) == 0) {
				printf("Calling allocateMem\n");				
				struct in6_addr remoteMemory = allocateMem(sockfd, p);
				inet_ntop(p->ai_family,(struct sockaddr *) &remoteMemory.s6_addr, s, sizeof s);
				printf("Got this pointer from call%s\n", s);
				memcpy(&remotePointers[count++], &remoteMemory, sizeof(remoteMemory));
			} else {
				int num = atoi(input);
				printf("Received %d as input\n", num);
				int j; 
				for (j = 0; j < num; j++) {
					printf("Calling allocateMem\n");
					struct in6_addr remoteMemory = allocateMem(sockfd, p);					
					inet_ntop(p->ai_family,(struct sockaddr *) &remoteMemory.s6_addr, s, sizeof s);
					printf("Got this pointer from call%s\n", s);
					printf("Creating pointer to remote memory address\n");
					memcpy(&remotePointers[count++], &remoteMemory, sizeof(remoteMemory));
				}
			}
		} else if (strcmp("L", input) == 0){
			printf("Remote Address Pointer\n");
			for (i = 0; i < 100; i++){
				if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
					inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
					printf("%s\n", s);
				}
			}
		} else if (strcmp("R", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to read a custom memory address. A to read all pointers.\n", input, sizeof(input));

			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
						printf("Using pointer %s to read\n", s);
						localData = getMemory(sockfd, p, &remotePointers[i]);
						printf("Retrieved Data (first 80 bytes): %.*s\n", 80, localData);
					}
				}
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) &pointer.s6_addr, s, sizeof s);
				printf("Reading from this pointer %s\n", s);
				localData = getMemory(sockfd, p, &pointer);
				printf(ANSI_COLOR_CYAN "Retrieved Data (first 80 bytes):\n");
				printf("****************************************\n");
				printf("\t%.*s\t\n",80, localData);
				printf("****************************************\n");
				printf(ANSI_COLOR_RESET);
			}
		} else if (strcmp("W", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to write to a custom memory address. A to write to all pointers.\n", input, sizeof(input));

			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);					
						printf("Writing to pointer %s\n", s);
						memset(input, 0, len);
						getLine("Please enter your data:\n", input, sizeof(input));
						if (strcmp("", input) == 0) {
							printf("Writing random bytes\n");
							char * payload = gen_rdm_bytestream(BLOCK_SIZE);
							writeToMemory(sockfd, p, payload, &remotePointers[i]);
						} else {
							printf(ANSI_COLOR_MAGENTA "Writing:\n");
							printf("****************************************\n");
							printf("\t%.*s\t\n",80, input);
							printf("****************************************\n");
							printf(ANSI_COLOR_RESET);
							writeToMemory(sockfd, p, input, &remotePointers[i]);	
						}
					}
				}
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) pointer.s6_addr, s, sizeof s);
				printf("Writing to pointer %s\n", s);
				memset(input, 0, len);
				getLine("Please enter your data:\n", input, sizeof(input));
				if (strcmp("", input) == 0) {
					printf("Writing random bytes\n");
					char * payload = gen_rdm_bytestream(BLOCK_SIZE);
					writeToMemory(sockfd, p, payload, &remotePointers[i]);
				} else {
					printf("Writing: %s\n", input);
					writeToMemory(sockfd, p, input, &remotePointers[i]);	
				}
			}
		} else if (strcmp("M", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to free a custom memory address. A to migrate all pointers.\n", input, sizeof(input));
			
			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);					
						memset(input, 0, len);
						printf("Migrating pointer %s\n", s);
						getLine("Please enter your migration machine:\n", input, sizeof(input));
						if (atoi(input) <= NUM_HOSTS) {
							printf("Migrating\n");
							migrate(sockfd, p, &remotePointers[i], atoi(input));
						} else {
							printf("FAILED\n");	
						}
					}
				}
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) pointer.s6_addr, s, sizeof s);
				printf("Migrating pointer %s\n", s);
				memset(input, 0, len);
				getLine("Please enter your migration machine:\n", input, sizeof(input));
				if (atoi(input) <= NUM_HOSTS) {
					printf("Migrating\n");
					migrate(sockfd, p, &pointer, atoi(input));
				} else {
					printf("FAILED\n");	
				}
			}
		} else if (strcmp("F", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to free a custom memory address. A to free all pointers.\n", input, sizeof(input));
			
			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
						printf("Freeing pointer %s\n", s);
						releaseMemory(sockfd, p, &remotePointers[i]);
						memset(remotePointers[i].s6_addr,0, IPV6_SIZE);
					}
				}	
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) &pointer.s6_addr, s, sizeof s);
				printf("Freeing pointer%s\n", s);				releaseMemory(sockfd, p, &pointer);
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						memset(remotePointers[i].s6_addr,0, IPV6_SIZE);
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
	free(lazyZero);
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
	int isAutoMode = 1;
	//Socket operator variables
	const int on=1, off=0;
	
	int c;
  	opterr = 0;
	while ((c = getopt (argc, argv, ":i")) != -1) {
	switch (c)
	  {
	  case 'i':
	    isAutoMode = 0;
	    break;
	  case '?':
	      fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	    return 1;
	  default:
	    abort ();
	  }
	}

	if (argc <= 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "::1";
		argv[2] = "5000";
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(NULL, argv[2], &hints, &servinfo)) != 0) {
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
		interactiveMode(sockfd, p);
	}
	freeaddrinfo(servinfo); // all done with this structure

	// TODO: send close message so the server exits
	close(sockfd);

	return 0;
}
