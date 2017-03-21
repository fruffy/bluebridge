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
#include <sys/types.h>
#include <sys/wait.h>

#include "538_utils.h"

/* 
 * get sockaddr, IPv4 or IPv6:
 */
void *get_in_addr(struct sockaddr *sa) {
	// socket family is IPv4
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}

	// socket family is IPv6
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

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
	unsigned char *stream = malloc(num_bytes);
	size_t i;

	for (i = 0; i < num_bytes; i++) {
		stream[i] = rand();
	}

	return stream;
}

/*
 * TODO: explain.
 * Also use s to get rid of the warning.
 */
void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}

/*
 * Sends message to specified socket
 */
void sendMsg(int sockfd, char * sendBuffer, int msgBlockSize) {
	if (send(sockfd, sendBuffer, msgBlockSize, 0) < 0)
		perror("ERROR writing to socket");
	memset(sendBuffer, 0, msgBlockSize);
}

/*
 * Receives message from socket
 */
int receiveMsg(int sockfd, char * receiveBuffer, int msgBlockSize) {
	//Sockets Layer Call: recv()
	int numbytes = 0;
	memset(receiveBuffer, 0, msgBlockSize);
	if ((numbytes = recv(sockfd, receiveBuffer, msgBlockSize, 0)) == -1) {
		perror("ERROR reading from socket");
		exit(1);
	}
	return numbytes;
}

/*
 * Prints byte buffer
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
