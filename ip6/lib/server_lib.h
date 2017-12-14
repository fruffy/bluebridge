#ifndef PROJECT_SERVER
#define PROJECT_SERVER
#include "network.h"

int allocate_mem(struct sockaddr_in6 *targetIP);
int allocate_mem_bulk(struct sockaddr_in6 *targetIP, uint64_t size);
int get_mem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int write_mem(char * receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int free_mem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);


#endif