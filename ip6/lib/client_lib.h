#ifndef PROJECT_CLIENT
#define PROJECT_CLIENT

#define MAX_HOSTS  5

#include <netdb.h>            // struct addrinfo
#include "network.h"


struct in6_memaddr allocate_rmem(struct sockaddr_in6 *targetIP);
struct in6_memaddr *allocate_rmem_bulk(struct sockaddr_in6 *targetIP, uint64_t size);
int write_rmem(struct sockaddr_in6 *targetIP, char *payload,  struct in6_memaddr *remoteAddr);
int free_rmem(struct sockaddr_in6 *targetIP,  struct in6_memaddr *remoteAddr);
int get_rmem(char *receiveBuffer, int length, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);

int write_raid_mem(struct sockaddr_in6 *targetIP, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed);
int read_raid_mem(struct sockaddr_in6 *targetIP, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed);
struct in6_addr *gen_rdm_ip6_target();
struct in6_addr *gen_ip6_target();
void set_host_list(struct in6_addr *host_addrs, int num_hosts);


struct in6_addr *get_ip6_target(uint8_t index);
int numHosts();

#endif
