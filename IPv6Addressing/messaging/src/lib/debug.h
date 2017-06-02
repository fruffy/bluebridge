#ifndef PROJECT_DEBUG
#define PROJECT_DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>


#define DEBUG 0

#define print_debug(format, args...); \
	if (DEBUG) {					 \
		printf("[DEBUG]: ");		 \
		printf(format, ## args);	 \
		printf("\n");				 \
	}								 \

//void print_debug(char* message);

int printBytes(char * receiveBuffer);

int printNBytes(char * receiveBuffer, int num);

int print_addrInfo(struct addrinfo *result);
struct timeval st, et;

#endif
