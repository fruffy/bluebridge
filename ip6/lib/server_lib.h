#ifndef PROJECT_SERVER
#define PROJECT_SERVER
#include "types.h"
#include "network.h"

int allocate_mem(struct sockaddr_in6 *target_ip);
int allocate_mem_bulk(struct sockaddr_in6 *target_ip, uint64_t size);
int read_mem(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
int write_mem(char * receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
int write_mem_bulk(char * receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
int free_mem(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
void process_request(struct sockaddr_in6 *targetIP, ip6_memaddr *remoteAddr, char *receiveBuffer, uint16_t size);
#endif