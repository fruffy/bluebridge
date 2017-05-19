#define _GNU_SOURCE


#include "538_utils.h"
#include "debug.h"
#include "udpcooked.h"

const int SUBNET_ID = 1;// 16 bits for subnet id
const int GLOBAL_ID = 33022;// 16 bits for link local id

const int NUM_HOSTS = 3; // number of hosts in the rack
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
	unsigned char *stream = (unsigned char *) malloc(num_bytes);
	size_t i;

	for (i = 0; i < num_bytes; i++) {
		stream[i] = rand();
	}

	return stream;
}

/*
 * Generates a random IPv6 address target under specific constraints
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr * gen_rdm_IPv6Target() {
	// Add the pointer

	struct in6_addr * newAddr = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
	uint8_t rndHost = (rand()% NUM_HOSTS)+1;
	/*// Insert link local id
	memcpy(newAddr->s6_addr,&GLOBAL_ID,2);*/
	// Insert subnet id
	memcpy(newAddr->s6_addr+4,&SUBNET_ID,1);
	//We are allocating from a random host
	memcpy(newAddr->s6_addr+5,&rndHost,1);


	char s[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6,newAddr, s, sizeof s);
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
	inet_ntop(AF_INET6,newAddr, s, sizeof s);
	print_debug("Target IPv6 Pointer %s",s);
	return newAddr;
}

/*
 * Sends message to specified socket
 * Simpler version where we do not need to insert the IPv6Addr into the header
 */
int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p) {

	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family,get_in_addr(p->ai_addr), s, sizeof s);
	print_debug("Sending to %s:%d", s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port));
	printNBytes(sendBuffer,20);
	cookUDP(sockfd, sendBuffer, msgBlockSize, p, 5000);	
/*	socklen_t slen = sizeof(struct sockaddr_in6);
	if (sendto(sockfd,sendBuffer,msgBlockSize,0, p->ai_addr, slen) < 0) {
		perror("ERROR writing to socket");
		return EXIT_FAILURE;
	}*/
	
	memset(sendBuffer, 0, msgBlockSize);
	return EXIT_SUCCESS;
}
/*
 * Sends message to specified socket
 */
//TODO Evaluate what variables and structures are actually needed here
//TODO: Error handling
int sendUDPIPv6(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p, struct in6_addr remotePointer) {
	
	char s[INET6_ADDRSTRLEN];
	/*
	inet_ntop(p->ai_family,get_in_addr(p->ai_addr), s, sizeof s);
	print_debug("Previous pointer... %s:%d",s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port) );*/
	
	memcpy(&(((struct sockaddr_in6*) p->ai_addr)->sin6_addr), &remotePointer, sizeof(remotePointer));
	p->ai_addrlen = sizeof(remotePointer);
	
	inet_ntop(p->ai_family,get_in_addr(p->ai_addr), s, sizeof s);
	print_debug("Inserting %u Pointer into packet header... %s:%d",p->ai_addrlen,s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port) );	
	printf("Sending...\n");
	printNBytes(sendBuffer, 30);
/*	socklen_t slen = sizeof(struct sockaddr_in6);
	if (sendto(sockfd,sendBuffer,msgBlockSize,0, p->ai_addr, slen) < 0) {
		perror("ERROR writing to socket");
		return EXIT_FAILURE;
	}*/
	cookUDP(sockfd, sendBuffer, msgBlockSize, p, 5000);
	memset(sendBuffer, 0, msgBlockSize);
	return EXIT_SUCCESS;
}


/*
 * Receives message from socket and also store the destination ip of the incoming packet
 * This will be our pointer or any additional command
 */
 //http://stackoverflow.com/questions/3062205/setting-the-source-ip-for-a-udp-socket
//TODO Evaluate what variables and structures are actually needed here
//TODO: Error handling
int receiveUDPIPv6(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p, struct in6_addr * ipv6Pointer) {

	struct sockaddr_in6 from;
	struct iovec iovec[1];
	struct msghdr msg;
	char msg_control[1024];
	char udp_packet[msgBlockSize];
	int numbytes = 0;
	char s[INET6_ADDRSTRLEN];
	iovec[0].iov_base = udp_packet;
	iovec[0].iov_len = sizeof(udp_packet);
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = iovec;
	msg.msg_iovlen = sizeof(iovec) / sizeof(*iovec);
	msg.msg_control = msg_control;
	msg.msg_controllen = sizeof(msg_control);
	msg.msg_flags = 0;

	print_debug("Waiting for response...\n");
	memset(receiveBuffer, 0, msgBlockSize);
	numbytes = recvmsg(sockfd, &msg, 0);
	struct in6_pktinfo * in6_pktinfo;
	struct cmsghdr* cmsg;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != 0; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
			in6_pktinfo = (struct in6_pktinfo*)CMSG_DATA(cmsg);
			inet_ntop(p->ai_family,&in6_pktinfo->ipi6_addr, s, sizeof s);
			print_debug("Received packet was sent to this IP %s",s);
			memcpy(ipv6Pointer->s6_addr,&in6_pktinfo->ipi6_addr,IPV6_SIZE);
			memcpy(receiveBuffer,iovec[0].iov_base,iovec[0].iov_len);
			memcpy(p->ai_addr, (struct sockaddr *) &from, sizeof(from));
			p->ai_addrlen = sizeof(from);
		}
	}

	inet_ntop(p->ai_family,(struct sockaddr *) get_in_addr(p->ai_addr), s, sizeof s);
	print_debug("Got message from %s:%d \n", s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port));
	printNBytes(receiveBuffer, 50);
	return numbytes;
}

/*
 * Receives message from socket
 * Simpler version where we do not need the fancy msghdr structure
 */
int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p) {

	int numbytes = 0;
	socklen_t slen = sizeof(struct sockaddr_in6);
	struct sockaddr_in6 sin;
	socklen_t len = sizeof(sin);
		if (getsockname(sockfd, (struct sockaddr_in6 *)&sin, &len) == -1)
		perror("getsockname");
	else
		printf("receiving on port number %d\n", ntohs(sin.sin6_port));

	memset(receiveBuffer, 0, msgBlockSize);
	print_debug("Waiting for response...\n");
	if ((numbytes = recvfrom(sockfd,receiveBuffer, msgBlockSize, 0, p->ai_addr,&slen)) == -1) {
		perror("ERROR reading from socket");
		exit(1);
	}
	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family,(struct sockaddr *) get_in_addr(p->ai_addr), s, sizeof s);
	print_debug("Got message from %s:%d \n", s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port));

	return numbytes;
}

/*
 * Get a POINTER_SIZE pointer from an IPV6_SIZE ip address 
 */
uint64_t getPointerFromIPv6(struct in6_addr addr) {
	uint64_t pointer = 0;
	memcpy(&pointer,addr.s6_addr+IPV6_SIZE-POINTER_SIZE, POINTER_SIZE);
	// printf("Converted IPv6 to Pointer: ");
	// printNBytes((char*) &pointer,POINTER_SIZE);
	return pointer;
}

/*
 * Convert a POINTER_SIZE bit pointer to a IPV6_SIZE bit IPv6 address\
 * (beginning at the POINTER_SIZEth bit)
 */
struct in6_addr getIPv6FromPointer(uint64_t pointer) {
	struct in6_addr * newAddr = (struct in6_addr *) calloc(1, sizeof(struct in6_addr));
	char s[INET6_ADDRSTRLEN];
	// printf("Memcpy in getIPv6FromPointer\n");
	memcpy(newAddr->s6_addr+IPV6_SIZE-POINTER_SIZE, (char *)pointer, POINTER_SIZE);
	memcpy(newAddr->s6_addr+4,&SUBNET_ID,1);
	inet_ntop(AF_INET6,newAddr, s, sizeof s);
	print_debug("IPv6 Pointer %s",s);
	return *newAddr;
}

/*
 * TODO: explain
 * Binds to the next available address?
 * Need to bind to INADDR_ANY instead
 */
struct addrinfo* bindSocket(struct addrinfo* p, struct addrinfo* servinfo, int* sockfd) {
	int blocking = 0;
	//Socket operator variables
	const int on=1, off=0;

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &blocking, sizeof(int))
				== -1) {
			perror("setsockopt");
			exit(1);
		}
		setsockopt(*sockfd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
		setsockopt(*sockfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
		setsockopt(*sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
		struct sockaddr_in6 *tempi = (struct sockaddr_in6 *) p->ai_addr;
		 tempi->sin6_addr = in6addr_any;
		if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(*sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	return p;
}