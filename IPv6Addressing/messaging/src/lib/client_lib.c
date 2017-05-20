/*
 ** client.c -- a stream socket client demo
 */

#include "client_lib.h"

/*
 * Sends message to allocate memory
 */
struct in6_addr allocateMem(int sockfd, struct addrinfo * p) {
	print_debug("Mallocing send and receive buffers");
	char  sendBuffer[BLOCK_SIZE];
	char  receiveBuffer[BLOCK_SIZE];

	// Lines are for ndpproxy DO NOT REMOVE
	struct in6_addr * ipv6Pointer = gen_rdm_IPv6Target();
	memcpy(&(((struct sockaddr_in6*) p->ai_addr)->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
	p->ai_addrlen = sizeof(*ipv6Pointer);	
	free(ipv6Pointer);

	memcpy(sendBuffer, ALLOC_CMD, sizeof(ALLOC_CMD));
	sendUDPRaw(sockfd, sendBuffer,BLOCK_SIZE, p);
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
		free(remotePointer);
	} else {
		print_debug("Response was not successful");
		// Not successful so we send another message?
		memcpy(sendBuffer, "What's up?", sizeof("What's up?"));
		//sendMsg(sockfd, sendBuffer, sizeof("What's up?"));
		sendUDPRaw(sockfd, sendBuffer,BLOCK_SIZE, p);
		memset(&retVal.s6_addr,0, IPV6_SIZE);

		// TODO: why do we return 0? Isn't there a better number 
		// (i.e. -1)

		print_debug("Keeping return value as 0");
	}
	//TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
	return retVal;
}
/*
 * Sends a write command to the sockfd for pointer remotePointer
 */
int writeToMemory(int sockfd, struct addrinfo * p, char * payload,  struct in6_addr * toPointer) {
	
	//TODO: Error handling
	char  sendBuffer[BLOCK_SIZE];
	char  receiveBuffer[BLOCK_SIZE];

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

	sendUDPIPv6Raw(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);


	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Releases the remote memory
 */
int releaseMemory(int sockfd, struct addrinfo * p,  struct in6_addr * toPointer) {
	char  sendBuffer[BLOCK_SIZE];
	char  receiveBuffer[BLOCK_SIZE];

	int size = 0;

	// Create message
	memcpy(sendBuffer+size, FREE_CMD, sizeof(FREE_CMD));
	size += sizeof(FREE_CMD) - 1; // Should be 5
	memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);

	if (DEBUG) {
		printf("Releasing Data with pointer: ");
		printNBytes((char*)toPointer->s6_addr,IPV6_SIZE);
	}

	// Send message
	sendUDPIPv6Raw(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);

	// Receive response
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);
	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Reads the remote memory
 */
char * getMemory(int sockfd, struct addrinfo * p, struct in6_addr * toPointer) {
	char  sendBuffer[BLOCK_SIZE];
	char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;

	// Prep message
	memcpy(sendBuffer, GET_CMD, sizeof(GET_CMD));
	size += sizeof(GET_CMD) - 1; // Should be 4
	memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);

	if (DEBUG) {
		printf("Retrieving Data with pointer: ");
		printNBytes((char*)toPointer,IPV6_SIZE);
	}

	// Send message
	sendUDPIPv6Raw(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	// Receive response
	print_debug("Now waiting")
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);

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
	free(receiveBuffer);
	return 0;
}


