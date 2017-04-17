#ifndef PROJECT_DEBUG
#define PROJECT_DEBUG



#define DEBUG 1

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

#endif