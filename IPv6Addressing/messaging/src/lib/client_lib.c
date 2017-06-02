/*
 ** client_lib.c -- 
 */

// TODO: Add a get remote machine method (needs to get the remote machine which holds a specific address.)

#include "client_lib.h"

/*
 * Allocate memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
// TODO: Especially for all send and receive calls
struct in6_addr allocateRemoteMem(int sockfd, struct addrinfo * p) {
	char  sendBuffer[BLOCK_SIZE];
	char  receiveBuffer[BLOCK_SIZE];

	// Generate a random IPv6 address out of a set of available hosts
	// TODO: Use config file
	struct in6_addr * ipv6Pointer = gen_rdm_IPv6Target();
	memcpy(&(((struct sockaddr_in6*) p->ai_addr)->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
	p->ai_addrlen = sizeof(*ipv6Pointer);	
	free(ipv6Pointer);
	// Send the command to the target host and wait for response
	memcpy(sendBuffer, ALLOC_CMD, sizeof(ALLOC_CMD));
	//sendUDP(sockfd, sendBuffer,BLOCK_SIZE, p);
	sendUDPRaw(sendBuffer,BLOCK_SIZE, p);

	receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);


	struct in6_addr retVal;
	if (memcmp(receiveBuffer,"ACK", 3) == 0) {
		// If the message is ACK --> successful allocation
		struct in6_addr * remotePointer = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
		
		if (DEBUG) {
			printf("Received pointer data: ");
			printNBytes(receiveBuffer+4,IPV6_SIZE);
		}
		// Copy the returned pointer (very precise offsets)
		memcpy(remotePointer, receiveBuffer+4, IPV6_SIZE);
		// Insert information about the source host (black magic)
		//00 00 00 00 01 02 00 00 00 00 00 00 00 00 00 00
		//			  ^  ^ these two bytes are stored (subnet and host ID)
		memcpy(remotePointer->s6_addr+4, (char *) get_in_addr(p->ai_addr)+4, 2);
		print_debug("Got: %p from server", (void *)remotePointer);

		retVal = *remotePointer;
		free(remotePointer);
	} else {
		printf("Response was not successful");
		// Not successful set the return address to zero.
		memset(&retVal.s6_addr,0, IPV6_SIZE);
	}
	return retVal;
}
/*
 * Sends a write command to the server based on toPointer
 */
// TODO: Implement meaningful return types and error messages
int writeRemoteMem(int sockfd, struct addrinfo * p, char * payload,  struct in6_addr * toPointer) {
	
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

	//sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	sendUDPIPv6Raw(sendBuffer,BLOCK_SIZE, p,*toPointer);
	receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);

	// TODO: change to be meaningful, i.e., error message
	return 0;
}

/*
 * Releases the remote memory based on toPointer
 */
// TODO: Implement meaningful return types and error messages
int freeRemoteMem(int sockfd, struct addrinfo * p,  struct in6_addr * toPointer) {
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

	// Send message and check if it was successful
	// TODO: Check if it actually was successful
	//sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	sendUDPIPv6Raw(sendBuffer,BLOCK_SIZE, p,*toPointer);
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);
	return 0;
}

/*
 * Reads the remote memory based on toPointer
 */
// TODO: Implement meaningful return types and error messages
char * getRemoteMem(int sockfd, struct addrinfo * p, struct in6_addr * toPointer) {
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
	// Send request and store response
	//sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
	sendUDPIPv6Raw(sendBuffer,BLOCK_SIZE, p,*toPointer);
	print_debug("Now waiting")
	receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);
	return receiveBuffer;
}

/*
 * Mirgrates? the remote memory
 */
int migrateRemoteMem(int sockfd, struct addrinfo * p, struct in6_addr * toPointer, int machineID) {
	char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer;
	// Allocates storage
	char * ovs_cmd = (char*)malloc(100 * sizeof(char));
	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family,(struct sockaddr *) toPointer->s6_addr, s, sizeof s);


	printf("Getting pointer\n");
	receiveBuffer = getRemoteMem(sockfd, p, toPointer);
	printf("Freeing pointer\n");

	freeRemoteMem(sockfd, p, toPointer);
	printf("Writing pointer\n");
	sprintf(ovs_cmd, "ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dst=%s,priority=65535,actions=output:%d", s, machineID);
	int status = system(ovs_cmd);
	printf("%d\t%s\n", status, ovs_cmd);

	writeRemoteMem(sockfd, p, receiveBuffer, toPointer);
	free(sendBuffer);
	free(ovs_cmd);
	free(receiveBuffer);
	return 0;
}


