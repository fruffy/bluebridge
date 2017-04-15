/*
 ** client.c -- a stream socket client demo
 */

#include "./lib/538_utils.h"
#include "./lib/debug.h"


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
//	6. Fix interactive mode and usability bugs
//	7. Switch to raw socket packets (hope is to get rid of NDP requests)
//	http://stackoverflow.com/questions/15702601/kernel-bypass-for-udp-and-tcp-on-linux-what-does-it-involve
//	https://austinmarton.wordpress.com/2011/09/14/sending-raw-ethernet-packets-from-a-specific-interface-in-c-on-linux/
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
		printf("Received pointer data: ");
		printNBytes(receiveBuffer+4,IPV6_SIZE);
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

	printf("Sending Data: %d bytes", size);
	printNBytes((char*) toPointer->s6_addr, IPV6_SIZE);
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

	printf("Releasing Data with pointer: ");
	printNBytes((char*)toPointer->s6_addr,IPV6_SIZE);

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

	printf("Retrieving Data with pointer: ");
	printNBytes((char*)toPointer,IPV6_SIZE);

	// Send message
	sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	// Receive response
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);


	free(sendBuffer);

	return receiveBuffer;
}

int basicOperations( int sockfd, struct addrinfo * p) {
	int i;
	// Initialize remotePointers array
	struct LinkedPointer * rootPointer = (struct LinkedPointer *) malloc( sizeof(struct LinkedPointer));
	struct LinkedPointer * nextPointer = rootPointer;
	//init the root element
	nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
	nextPointer->AddrString = allocateMem(sockfd, p);
	for (i = 0; i < 999; i++) {
		srand(time(NULL));

		nextPointer = nextPointer->Pointer;
		nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
		nextPointer->AddrString = allocateMem(sockfd, p);
		//printNBytes((char *) rootPointer->AddrString.s6_addr, 16);
		//printNBytes((char *) nextPointer->AddrString.s6_addr, 16);
	}
	// don't point to garbage
	// temp fix
	nextPointer->Pointer = NULL;
	
	i = 1;
	while(rootPointer != NULL)	{

		printf("Iteration %d\n", i);
		struct in6_addr remoteMemory = rootPointer->AddrString;
		printf("Using Pointer: %p\n", (void *) getPointerFromIPv6(rootPointer->AddrString));
		print_debug("Creating payload");
		srand(time(NULL));
		char * payload = gen_rdm_bytestream(BLOCK_SIZE);
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

	if (argc <= 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "::1";
		argv[2] = "5000";
	}

	//Routing configuration
	// This is a temporary solution to enable the forwarding of unknown ipv6 subnets
	//printf("%d\n",system("sudo ip -6 route add local 0:0100::/40  dev lo"));
	//ip -6 route add local ::0100:0:0:0:0/64  dev h1-eth0
	//ip -6 route add local ::0100:0:0:0:0/64  dev h2-eth0
	// Tells the getaddrinfo to only return sockets
	// which fit these params.
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
