#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), malloc, and calloc()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "utils.h"

/*
 * Gets the line from the command prompt
 * http://stackoverflow.com/questions/4023895/how-to-read-string-entered-by-user-in-c
 */
int getLine(char *prmpt, char *buff, size_t sz) {
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

/*
 * Gets random byte array with size num_bytes
 */
unsigned char *gen_rdm_bytestream(size_t num_bytes) {
	unsigned char *stream = (unsigned char *) malloc(num_bytes);
	size_t i;

	for (i = 0; i < num_bytes; i++) {
		stream[i] = rand();
	}

	return stream;
}

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
int printNBytes(void *receiveBuffer, int num) {
	int i;

	// for (i = num-1; i>=0; i--) {
	// 	printf("%02x", (unsigned char) receiveBuffer[i]);
	// }
	for (i =0; i<=num-1; i++) {
		printf("%02x", ((unsigned char *)receiveBuffer)[i]);
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

