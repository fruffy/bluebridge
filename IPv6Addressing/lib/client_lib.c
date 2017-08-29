/*
 ** client_lib.c -- 
 */

// TODO: Add a get remote machine method (needs to get the remote machine which holds a specific address.)

#include "client_lib.h"
#include "utils.h"
#include <string.h>           // strcpy, memset(), and memcpy()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <stdio.h>            // printf() and sprintf()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()


/*
 * Generates a random IPv6 address target under specific constraints
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr *gen_rdm_IPv6Target() {
    
    // Add the pointer
    struct in6_addr * newAddr = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
    uint8_t rndHost = (rand()% NUM_HOSTS)+1;
    if (rndHost == 1) rndHost++;
    /*// Insert link local id
    memcpy(newAddr->s6_addr,&GLOBAL_ID,2);*/
    // Insert subnet id
    memcpy(newAddr->s6_addr+4,&SUBNET_ID,1);
    //We are allocating from a random host
    memcpy(newAddr->s6_addr+5,&rndHost,1);

    char s[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, newAddr, s, sizeof s);
    print_debug("Target IPv6 Pointer %s",s);
    return newAddr;
}

/*
 * Generates a fixed IPv6 address target based on the provides int value
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr * gen_fixed_IPv6Target(uint8_t rndHost) {
    // Add the pointer

    struct in6_addr * newAddr = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
    /*// Insert link local id
    memcpy(newAddr->s6_addr,&GLOBAL_ID,2);*/
    // Insert subnet id
    memcpy(newAddr->s6_addr+4,&SUBNET_ID,1);
    //We are allocating from a random host
    memcpy(newAddr->s6_addr+5,&rndHost,1);


    char s[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, newAddr, s, sizeof s);
    print_debug("Target IPv6 Pointer %s",s);
    return newAddr;
}

/*
 * Allocate memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
// TODO: Especially for all send and receive calls
struct in6_addr allocateRemoteMem(struct sockaddr_in6 * targetIP) {
	char  sendBuffer[BLOCK_SIZE];
	char  receiveBuffer[BLOCK_SIZE];

	// Generate a random IPv6 address out of a set of available hosts
	// TODO: Use config file
	struct in6_addr * ipv6Pointer = gen_rdm_IPv6Target();
	memcpy(&(targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
	free(ipv6Pointer);
	// Send the command to the target host and wait for response
	memcpy(sendBuffer, ALLOC_CMD, sizeof(ALLOC_CMD));
	sendUDPRaw(sendBuffer,BLOCK_SIZE, targetIP);
	receiveUDPIPv6Raw(receiveBuffer,BLOCK_SIZE, targetIP, NULL);

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
		memcpy(remotePointer->s6_addr+4, ((char*)&targetIP->sin6_addr) +4, 2);
		print_debug("Got: %p from server", (void *)remotePointer);

		retVal = *remotePointer;
		free(remotePointer);
	} else {
		printf("Response was not successful\n");
		// Not successful set the return address to zero.
		memset(&retVal.s6_addr,0, IPV6_SIZE);
	}
	return retVal;
}
/*
 * Sends a write command to the server based on toPointer
 */
// TODO: Implement meaningful return types and error messages
int writeRemoteMem(struct sockaddr_in6 * targetIP, char * payload,  struct in6_addr * toPointer) {
	
	//TODO: Error handling
	char sendBuffer[BLOCK_SIZE];
	char receiveBuffer[BLOCK_SIZE];

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

	sendUDPIPv6Raw(sendBuffer, BLOCK_SIZE, targetIP, *toPointer);
	receiveUDPIPv6Raw(receiveBuffer,BLOCK_SIZE, targetIP, NULL);
	// TODO: change to be meaningful, i.e., error message
	return 0;
}

/*
 * Releases the remote memory based on toPointer
 */
// TODO: Implement meaningful return types and error messages
int freeRemoteMem(struct sockaddr_in6 * targetIP,  struct in6_addr * toPointer) {
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
	sendUDPIPv6Raw(sendBuffer,BLOCK_SIZE, targetIP,*toPointer);
	receiveUDPIPv6Raw(receiveBuffer,BLOCK_SIZE, targetIP, NULL);
	return 0;
}

/*
 * Reads the remote memory based on toPointer
 */
// TODO: Implement meaningful return types and error messages
char * getRemoteMem(struct sockaddr_in6 * targetIP, struct in6_addr * toPointer) {
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
	sendUDPIPv6Raw(sendBuffer,BLOCK_SIZE, targetIP,*toPointer);
	print_debug("Now waiting")
	receiveUDPIPv6Raw(receiveBuffer,BLOCK_SIZE, targetIP, NULL);
	return receiveBuffer;
}

/*
 * Migrates remote memory
 */
int migrateRemoteMem(struct sockaddr_in6 * targetIP, struct in6_addr * toPointer, int machineID) {
	char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
	char * receiveBuffer;
	// Allocates storage
	char * ovs_cmd = (char*)malloc(100 * sizeof(char));
	char s[INET6_ADDRSTRLEN];
	inet_ntop(targetIP->sin6_family,(struct sockaddr *) toPointer->s6_addr, s, sizeof s);


	printf("Getting pointer\n");
	receiveBuffer = getRemoteMem(targetIP, toPointer);
	printf("Freeing pointer\n");

	freeRemoteMem(targetIP, toPointer);
	printf("Writing pointer\n");
	sprintf(ovs_cmd, "ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dst=%s,priority=65535,actions=output:%d", s, machineID);
	int status = system(ovs_cmd);
	printf("%d\t%s\n", status, ovs_cmd);

	writeRemoteMem(targetIP, receiveBuffer, toPointer);
	free(sendBuffer);
	free(ovs_cmd);
	free(receiveBuffer);
	return 0;
}


