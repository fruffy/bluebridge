#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "debug.h"

/*void print_debug(char* message) {
	if (DEBUG) {
		printf("[DEBUG]: %s\n", message);
	}
}*/
/*
 * Prints byte buffer until it hits terminating character
 */
int printBytes(char * receiveBuffer) {
	int i = 0;

	while(receiveBuffer[i] != '\0') {
		printf("%02x", (unsigned char) receiveBuffer[i]);
		i++;
	}
	printf("\n");
	return i;
}
/*
 * Print reverse byte buffer including specified length
 */
int printNBytes(char * receiveBuffer, int num) {
	int i;

	for (i = num; i>=0; i--) {
		printf("%02x", (unsigned char) receiveBuffer[i]);
	}
	printf("\n");
	return i;
}
/*
 * Prints a formatted representation of the addrinfo structure
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms737530(v=vs.85).aspx
 */
int print_addrInfo(struct addrinfo *result) {
	struct addrinfo *ptr = NULL;
	int i = 0;
	
	// Retrieve each address and print out the hex bytes
	for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

		printf("getaddrinfo response %d\n", i++);
		printf("\tFlags: 0x%x\n", ptr->ai_flags);
		printf("\tFamily: ");
		switch (ptr->ai_family) {
			case AF_UNSPEC:
				printf("Unspecified\n");
				break;
			case AF_INET:
				printf("AF_INET (IPv4)\n");
				printf("\tIPv4 address %s\n", inet_ntoa(((struct sockaddr_in *) ptr->ai_addr)->sin_addr));
				break;
			case AF_INET6:
				printf("AF_INET6 (IPv6)\n");
				char s[INET6_ADDRSTRLEN];
				inet_ntop(ptr->ai_family,(struct sockaddr_in6 *) ptr->ai_addr , s, sizeof s);
				printf("\tIPv6 address %s\n", s);
				break;
			default:
				printf("Other %d\n", ptr->ai_family);
				break;
		}
		printf("\tSocket type: ");
		switch (ptr->ai_socktype) {
			case 0:
				printf("Unspecified\n");
				break;
			case SOCK_STREAM:
				printf("SOCK_STREAM (stream)\n");
				break;
			case SOCK_DGRAM:
				printf("SOCK_DGRAM (datagram) \n");
				break;
			case SOCK_RAW:
				printf("SOCK_RAW (raw) \n");
				break;
			case SOCK_RDM:
				printf("SOCK_RDM (reliable message datagram)\n");
				break;
			case SOCK_SEQPACKET:
				printf("SOCK_SEQPACKET (pseudo-stream packet)\n");
				break;
			default:
				printf("Other %d\n", ptr->ai_socktype);
				break;
		}
		printf("\tProtocol: ");
		switch (ptr->ai_protocol) {
			case 0:
				printf("Unspecified\n");
				break;
			case IPPROTO_TCP:
				printf("IPPROTO_TCP (TCP)\n");
				break;
			case IPPROTO_UDP:
				printf("IPPROTO_UDP (UDP)\n");
				break;
			default:
				printf("Other %d\n", ptr->ai_protocol);
				break;
		}
		printf("\tLength of this sockaddr: %d\n", ptr->ai_addrlen);
		printf("\tCanonical name: %s\n", ptr->ai_canonname);
	}

	return 0;
}