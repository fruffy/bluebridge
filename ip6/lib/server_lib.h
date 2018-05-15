#ifndef PROJECT_SERVER
#define PROJECT_SERVER
#include "types.h"
#include "network.h"

int allocate_mem(struct sockaddr_in6 *target_ip);
int allocate_mem_bulk(struct sockaddr_in6 *target_ip, uint64_t size);
int read_mem(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
int write_mem(uint8_t * rx_buffer, struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
int write_mem_bulk(uint8_t * rx_buffer, struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
int free_mem(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr);
int trim_mem(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr, uint64_t size);
void process_request(struct sockaddr_in6 *target_ip, ip6_memaddr *remoteAddr, uint8_t *rx_buffer, uint16_t size);
#endif