#ifndef PROJECT_UTILS
#define PROJECT_UTILS

#define DEBUG 0
#define BLOCK_SIZE 1500 // max number of bytes we can get at once

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

#endif
