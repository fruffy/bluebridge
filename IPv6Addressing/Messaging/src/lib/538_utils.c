#define _GNU_SOURCE

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

/*void print_debug(char* message) {
	if (DEBUG) {
		printf("[DEBUG]: %s\n", message);
	}
}*/

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

char * get_rdm_string(size_t num_bytes, int index) {
	char* stream = malloc(num_bytes);
	char *string = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";

	sprintf(stream, "%d:", index);

	size_t i = strlen(stream);
	for (; i < num_bytes; i++) {
		stream[i] = string[rand()%strlen(string)];
	}
	return stream;
}

/*
 * Sends message to specified socket
 */
int sendTCP(int sockfd, char * sendBuffer, int msgBlockSize) {
	if (send(sockfd, sendBuffer, msgBlockSize, 0) < 0) {
		perror("ERROR writing to socket");
		return EXIT_FAILURE;
	}
	memset(sendBuffer, 0, msgBlockSize);
	return EXIT_SUCCESS;
}

/*
 * Receives message from socket
 */
int receiveTCP(int sockfd, char * receiveBuffer, int msgBlockSize) {
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
 * Sends message to specified socket
 */
int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p) {
	char s[INET6_ADDRSTRLEN];
	//wait for incoming connection
	inet_ntop(p->ai_family,get_in_addr(p->ai_addr), s, sizeof s);
	socklen_t slen = sizeof(struct sockaddr_in6);
	printf("Sending to %s:%d \n", s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port));
	if (sendto(sockfd,sendBuffer,msgBlockSize,0, p->ai_addr, slen) < 0) {
		perror("ERROR writing to socket");
		return EXIT_FAILURE;
	}
	memset(sendBuffer, 0, msgBlockSize);
	return EXIT_SUCCESS;
}

/*
 * Receives message from socket
 */
//http://stackoverflow.com/questions/3062205/setting-the-source-ip-for-a-udp-socket
int receiveUDPLegacy(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p) {
	//Sockets Layer Call: recv()
	int numbytes = 0;
	char s[INET6_ADDRSTRLEN];
	socklen_t slen = sizeof(struct sockaddr_in6);

	memset(receiveBuffer, 0, msgBlockSize);
	if ((numbytes = recvfrom(sockfd,receiveBuffer, msgBlockSize, 0, p->ai_addr,&slen)) == -1) {
		perror("ERROR reading from socket");
		exit(1);
	}
	//wait for incoming connection
	inet_ntop(p->ai_family,(struct sockaddr *) get_in_addr(p->ai_addr), s, sizeof s);
	printf("Got message from %s:%d \n", s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port));
	return numbytes;
}
int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p) {

	struct sockaddr_in6 from;
	struct iovec iovec[1];
	struct msghdr msg;
	char msg_control[1024];
	char udp_packet[msgBlockSize];
	int numbytes = 0;

	iovec[0].iov_base = udp_packet;
	iovec[0].iov_len = sizeof(udp_packet);
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = iovec;
	msg.msg_iovlen = sizeof(iovec) / sizeof(*iovec);
	msg.msg_control = msg_control;
	msg.msg_controllen = sizeof(msg_control);
	msg.msg_flags = 0;
	numbytes = recvmsg(sockfd, &msg, 0);
	//struct in_pktinfo in_pktinfo;
	struct in6_pktinfo * in6_pktinfo;
	//int have_in_pktinfo = 0;
	//int have_in6_pktinfo = 0;
	struct cmsghdr* cmsg;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != 0; cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
/*	  if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
	    in_pktinfo = *(struct in_pktinfo*)CMSG_DATA(cmsg);
	   // have_in_pktinfo = 1;
	  }*/

	  if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
	  {
	    in6_pktinfo = (struct in6_pktinfo*)CMSG_DATA(cmsg);
	    //have_in6_pktinfo = 1;
	    char s[INET6_ADDRSTRLEN];
	    inet_ntop(p->ai_family,&in6_pktinfo->ipi6_addr, s, sizeof s);
		printf("Message was sent to %s:%d \n",s, ntohs(((struct sockaddr_in6 *)&in6_pktinfo->ipi6_addr)->sin6_port));
		memcpy(receiveBuffer,iovec[0].iov_base,iovec[0].iov_len);
		p->ai_addr = (struct sockaddr *) &from;
	  }
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
