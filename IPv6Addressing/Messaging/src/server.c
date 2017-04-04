/*
 ** server.c -- a stream socket server demo
 */

#include "./lib/538_utils.h"
#include "./lib/debug.h"

#define BACKLOG 10     // how many pending connections queue will hold

char *varadr_char[1000];
int countchar = 0;

/*
 * Frees global memory
 */
int cleanMemory() {
	int i;
	for (i = 0; i <= countchar; i++) {
		free(varadr_char[i]);
	}

	return EXIT_SUCCESS;
}

/*
 * Adds a character to the global memory
 */
int addchar(char* charadr) {
	if (charadr == NULL ) {
		perror("\nError when trying to allocate a pointer! \n");
		cleanMemory();
		exit(EXIT_FAILURE);
	}

	varadr_char[countchar] = charadr;
	countchar++;
	return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Creates a new pointer to a new part of memory then sends it?
 */
int providePointer (int sock_fd, char * receiveBuffer, struct addrinfo * p) {
	print_debug("Creating allocated and sendBuffer with malloc");
	char * allocated = calloc(BLOCK_SIZE,sizeof(char));
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));

	int size = 0;
	printf("Input pointer: %p\n", (void *) allocated);
	struct in6_addr temp = getIPv6FromPointer((uint64_t) &allocated);
	uint64_t newpointer = getPointerFromIPv6(temp);

	//printf("New pointer: %" PRIx64 ", Old pointer: %" PRIx64 "\n", newpointer, pointer);
	printf("New pointer: %p, Old pointer: %p\n", (void *) newpointer, (void *) allocated);

	print_debug("Constructing message in sendBuffer");
	memcpy(sendBuffer+size, "ACK:", sizeof("ACK:"));
	size += sizeof("ACK:") - 1;
	memcpy(sendBuffer+size, &allocated, sizeof(allocated));
	size += sizeof(allocated);

	// char message[100] = {};
	// sprintf(message, "End sendBuffer size: %d", size);
	print_debug("End sendBuffer size: %d", size);

	//printBytes(sendBuffer);
	//printf("Interpretation of Server %s \n", sendBuffer);
	print_debug("Sending message");
	//sendMsg(sock_fd, sendBuffer, size);
	sendUDP(sock_fd, sendBuffer, BLOCK_SIZE, p);

	printf("\nAllocated Pointer: %p -> %s\n",allocated, allocated);

	print_debug("Freeing sendBuffer");
	free(sendBuffer);

	print_debug("Returning dummy value");
	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int writeMem (int sock_fd, char * receiveBuffer, struct addrinfo * p) {
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));
	char * target;
	printf("\nAllocated Pointer: %p -> %s\n",sendBuffer, sendBuffer);

	// TODO: why is this +9?
	char * dataToWrite = receiveBuffer + 9;
	
	// char * message2 = malloc(BLOCK_SIZE *sizeof(char));
	// sprintf(message2, "Data received (first 80 bytes): %.*s\n", 80, dataToWrite);
	print_debug("Data received (first 80 bytes): %.*s", 80, dataToWrite);
	
	// Copy the first eight bytes of receive buffer into the target
	memcpy(&target, receiveBuffer, POINTER_SIZE);

	// char message3[100] = {};
	// sprintf(message3, "Target pointer: %p", (void*) target);
	print_debug("Target pointer: %p", (void *) target);

	//printf("Target Pointer: %p -> %s\n",target, target);

	memcpy(target, dataToWrite, strlen(dataToWrite));

	printf("Content length %lu is stored at %p!\n", strlen(target), (void*)target);
	printf("First 80 bytes of content: %.*s\n\n", 80, target);
	memcpy(sendBuffer, "ACK", sizeof("ACK"));
	// Send the sendBuffer to the new fd (socket), only send 3 bytes.
	// TODO: shouldn't this be sizeof("ACK")?
	//sendMsg(sock_fd,sendBuffer,3);
	sendUDP(sock_fd, sendBuffer, BLOCK_SIZE, p);

	//free(sendBuffer);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * TODO: explain.
 * This is freeing target memory?
 */
int freeMem (int sock_fd, char * receiveBuffer, struct addrinfo * p) {
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));
	char * target;
	// Why copy when you're freeing the memory?
	memcpy(&target, receiveBuffer, POINTER_SIZE);

	printf("Content stored at %p has been freed!\n", (void*)target);
	free(target);
	memcpy(sendBuffer, "ACK", sizeof("ACK"));
	// Send the sendBuffer to the new fd (socket), only send 3 bytes.
	// TODO: shouldn't this be sizeof("ACK")?
	//sendMsg(sock_fd,sendBuffer,3);
	sendUDP(sock_fd, sendBuffer, BLOCK_SIZE, p);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Gets memory and sends it
 */
int getMem (int sock_fd, char * receiveBuffer, struct addrinfo * p) {
	char * sendBuffer = calloc(BLOCK_SIZE,sizeof(char));
	char * target;
	printBytes(receiveBuffer);
	// Copy eight bytes of the receiveBuffer into the target
	memcpy(&target, receiveBuffer, POINTER_SIZE);

	printf("Content length %lu is currently stored at %p!\n", strlen(target), (void*)target);

	printf("Content preview (80 bytes): %.*s\n", 80, target);
	printf("Content length %lu will be delivered to client!\n", strlen(target));
	// Copy the target into the send buffer (entire BLOCK_SIZE)
	// TODO: why do this if target only has 8 bytes?
	memcpy(sendBuffer, target, BLOCK_SIZE);

	// Send the sendBuffer (entire BLOCK_SIZE) to sock_fd
	//sendMsg(sock_fd,sendBuffer,BLOCK_SIZE);
	sendUDP(sock_fd, sendBuffer, BLOCK_SIZE, p);
	free(sendBuffer);

	// TODO change to be meaningful, i.e., error message
	return 0;
}

/*
 * Request handler for socket sock_fd
 * TODO: get message format
 */
void handleClientRequests(int sock_fd,	struct addrinfo * p) {
	char * receiveBuffer = calloc(BLOCK_SIZE,sizeof(char));
	int numbytes;
	char * splitResponse;

	while (1) {
		printf("Waiting for client message...\n");
		// Get the message
		//numbytes = receiveMsg(sock_fd, receiveBuffer, BLOCK_SIZE);
		char s[INET6_ADDRSTRLEN];
		numbytes = receiveUDP(sock_fd, receiveBuffer, BLOCK_SIZE, p);
		inet_ntop(p->ai_family,(struct sockaddr *) get_in_addr(p->ai_addr), s, sizeof s);
		printf("Got message from %s:%d \n", s,ntohs(((struct sockaddr_in6*) p->ai_addr)->sin6_port));
		// Parse the message (delimited by :)
		printf("Message from client: %i bytes\n", numbytes);
		printBytes(receiveBuffer);

		//char * dataToWrite = strtok(receiveBuffer, ":");
		//printf("Client Command: %s\n", dataToWrite);
		// Switch on the client command
		if (memcmp(receiveBuffer, ALLOC_CMD,2) == 0) {
			printf("Allocating...\n");
			splitResponse = receiveBuffer+2;
			providePointer(sock_fd, splitResponse, p);
		} else if (memcmp(receiveBuffer, WRITE_CMD,2) == 0) {
			splitResponse = receiveBuffer+2;
			printf("Writing to pointer: ");
			printNBytes(splitResponse, POINTER_SIZE);
			writeMem(sock_fd, splitResponse, p);
		} else if (memcmp(receiveBuffer, FREE_CMD,2) == 0) {
			splitResponse = receiveBuffer+2;
			printf("Deleting pointer: ");
			printNBytes(splitResponse,POINTER_SIZE);
			freeMem(sock_fd, splitResponse, p);
		} else if (memcmp(receiveBuffer, GET_CMD,2) == 0) {
			splitResponse = receiveBuffer+2;
			printf("Retrieving data: ");
			printNBytes(splitResponse,POINTER_SIZE);
			getMem(sock_fd, splitResponse, p);
		} else {
			// TODO: what is this doing?
			printf("Cannot match command!\n");
			if (sendUDP(sock_fd, "Hello, world!", 13, p) == -1) {
				perror("ERROR writing to socket");
				exit(1);
			}
		}
	}
}

/*
 * TODO: explain
 * Binds to the next available address?
 */
struct addrinfo* bindSocket(struct addrinfo* p, struct addrinfo* servinfo,
		int* sockfd) {
	int blocking = 0;

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
		const int on=1, off=0;

		setsockopt(*sockfd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
		setsockopt(*sockfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
		setsockopt(*sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

		if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(*sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	return p;
}

/*
 * Accept a connection on sockfd
 */
int acceptConnections(int sockfd) {
	// connector's address information
	struct sockaddr_storage their_addr; 
	char s[INET6_ADDRSTRLEN];
	socklen_t sin_size = sizeof their_addr;
	//wait for incoming connection
	int temp_fd = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size);
	inet_ntop((&their_addr)->ss_family,
			get_in_addr((struct sockaddr*) &their_addr), s, sizeof s);

	printf("server: got connection from %s\n", s);

	return temp_fd;
}

/*
 * Main workhorse method. Parses command args and does setup.
 * Blocks waiting for connections.
 */
int main(int argc, char *argv[]) {
	int sockfd;  // listen on sock_fd, new connection on sock_fd
	struct addrinfo hints, *servinfo;
	int rv;

	// hints = specifies criteria for selecting the socket address
	// structures
	memset(&hints, 0, sizeof(hints));
	if (argc < 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "5000";
		hints.ai_flags = AI_PASSIVE; // use my IP

	}
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	struct addrinfo *p;

	// loop through all the results and bind to the first we can
	p = bindSocket(p, servinfo, &sockfd);

/*		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
*/

	// Frees the memory allocated for servinfo (list of possible sockets)
	//freeaddrinfo(servinfo);

	// TODO: should free hints as well?
	// -> not a pointer

	// Check to make sure the server was able to bind
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
	// Listen on the socket with a queue size of BACKLOG
/*	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}*/



	// Start waiting for connections
	while (1) {

		//int sock_fd = acceptConnections(sockfd);
		/*if (sock_fd == -1) {
			perror("accept");
			exit(1);
		}*/
		//if (!fork()) {
			// fork the process
		//	close(sockfd); // child doesn't need the listener

		handleClientRequests(sockfd, p);
		//}

		//close(sock_fd); // parent doesn't need this
	}
	freeaddrinfo(p);
	close(sockfd);
	return 0;
}
