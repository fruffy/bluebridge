#ifndef PROJECT_UTILS
#define PROJECT_UTILS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <signal.h>


#define DEBUG 0
// <<<<<<< HEAD
// #define BLOCK_SIZE 4000 // max number of bytes we can get at once
#define GLOBAL_ID "EA75:DB1A:68D8" // 48 bits for global id
#define SUBNET_ID "C75F" // 16 bits for subnet id
// =======
#define BLOCK_SIZE 4096 // max number of bytes we can get at once
// >>>>>>> acd1836cd31c2145c1ed685eaa8afef3599bcb70

#define print_debug(format, args...)		\
			if (DEBUG) {					\
				printf("[DEBUG]: ");		\
				printf(format, ## args);	\
				printf("\n");				\
			}								\

//void print_debug(char* message);

void *get_in_addr(struct sockaddr *sa);

int getLine(char *prmpt, char *buff, size_t sz);

unsigned char *gen_rdm_bytestream(size_t num_bytes);

char *get_rdm_string(size_t num_bytes, int index);

int sendTCP(int sockfd, char * sendBuffer, int msgBlockSize);

int receiveTCP(int sockfd, char * receiveBuffer, int msgBlockSize);

int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p);

int receiveUDPLegacy (int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);
int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);


int printBytes(char * receiveBuffer);

uint64_t getPointerFromString(char* input);

uint64_t getPointerFromIPv6(struct in6_addr addr);

struct in6_addr getIPv6FromPointer(uint64_t pointer);

#endif
