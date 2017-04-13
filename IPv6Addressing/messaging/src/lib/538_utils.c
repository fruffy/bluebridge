#define _GNU_SOURCE


#include "538_utils.h"
#include "debug.h"
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
 * Gets random string array with size num_bytes
 */
//TODO Remove?
char * get_rdm_string(size_t num_bytes, int index) {
	char* stream = (char *) malloc(num_bytes);
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
 * Simpler version where we do not need to insert the IPv6Addr into the header
 */
int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p) {
	char s[INET6_ADDRSTRLEN];
	//wait for incoming connection
	inet_ntop(p->ai_family,get_in_addr(p->ai_addr), s, sizeof s);
	socklen_t slen = sizeof(struct sockaddr_in6);
	print_debug("Sending to %s:%d", s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port));
	if (sendto(sockfd,sendBuffer,msgBlockSize,0, p->ai_addr, slen) < 0) {
		perror("ERROR writing to socket");
		return EXIT_FAILURE;
	}
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
	inet_ntop(p->ai_family,get_in_addr(p->ai_addr), s, sizeof s);
	print_debug("Previous pointer... %s:%d",s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port) );
	
	memcpy(&(((struct sockaddr_in6*) p->ai_addr)->sin6_addr), &remotePointer, sizeof(remotePointer));
	p->ai_addrlen = sizeof(remotePointer);
	
	inet_ntop(p->ai_family,get_in_addr(p->ai_addr), s, sizeof s);
	print_debug("Inserting %u Pointer into packet header... %s:%d",p->ai_addrlen,s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port) );	
	
	socklen_t slen = sizeof(struct sockaddr_in6);
	if (sendto(sockfd,sendBuffer,msgBlockSize,0, p->ai_addr, slen) < 0) {
		perror("ERROR writing to socket");
		return EXIT_FAILURE;
	}
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

	return numbytes;
}



/*
 * Receives message from socket
 * Simpler version where we do not need the fancy msghdr structure
 */
int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p) {

	int numbytes = 0;
	socklen_t slen = sizeof(struct sockaddr_in6);

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
//TODO: Remove?
/*struct in6_addr getIPv6FromPointerStr(uint64_t pointer) {
	char* string_addr = (char*) calloc(IPV6_SIZE, sizeof(char));

	// Create the beginning of the address
	strcat(string_addr, GLOBAL_ID);
	strcat(string_addr, ":");
	strcat(string_addr, SUBNET_ID);
	strcat(string_addr, ":");// Pads the pointer

	// Add the pointer
	char* pointer_string = (char*) malloc(POINTER_SIZE * sizeof(char));
	
	//sprintf(pointer_string, "%" PRIx64, pointer);

	memcpy(&pointer_string, &pointer,POINTER_SIZE);
	printf("Pointer: %s\n", pointer_string);
	printf("Address so far: %s\n", string_addr);
	
	print_debug("Pointer length: %lu", strlen(pointer_string));
	print_debug("Address length: %lu", strlen(string_addr));

	unsigned int i;

	for (i = 0; i < strlen(pointer_string); i+=4) {
		char* substr = (char *) malloc(4 * sizeof(char));
		strcat(string_addr, ":");
		print_debug("Creating copy");
		strncpy(substr, pointer_string+i, 4);
		print_debug("Copy: %s", substr);
		print_debug("Performing concatenation");
		strcat(string_addr, substr);
		//strcat(string_addr, pointer_string[i]);
		//strcat(string_addr, pointer_string[i+1]);
		//strcat(string_addr, pointer_string[i+2]);
		//strcat(string_addr, pointer_string[i+3]);
	}

	print_debug("New address: %s", string_addr);

	struct in6_addr newAddr;

	if (inet_pton(AF_INET6, string_addr, &newAddr) == 1) {
		printf("SUCCESS: %s\n", string_addr);
		//successfully parsed string into "result"
	} else {
		printf("ERROR: not a valid address [%s]\n", string_addr);
		//failed, perhaps not a valid representation of IPv6?
	}

	return newAddr;
}*/

uint64_t getPointerFromString(char* input) {
	uint64_t address = 0;
	// printf("Memcpy in getPointerFromString\n");
	memcpy(&address, &input, POINTER_SIZE);
	uint64_t pointer = address;
	// printf("Returning pointer\n");
	return pointer;
}

//TODO: Remove?
uint64_t getPointerFromIPv6Str(struct in6_addr addr) {
	char* pointer = (char *) malloc(12 * sizeof(unsigned char));
	char str[INET6_ADDRSTRLEN];
	unsigned int i;
	inet_ntop(AF_INET6, addr.s6_addr, str, INET6_ADDRSTRLEN);
	printf("String address: %s\n", str);
	int j = 0;
	for (i = strlen(str) - 14; i < strlen(str); i++) { // 14 b/c :
		if (str[i] != ':') {
			pointer[j] = str[i];
			j++;
		}
	}
	printf("Pointer: %s\n", pointer);
	return getPointerFromString(pointer);
}

/*
 * Sends message to specified socket
 */
//TODO: Remove? Do we still need TCP?
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
//TODO: Remove? Do we still need TCP?
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