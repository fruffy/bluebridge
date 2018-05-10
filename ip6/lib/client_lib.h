#ifndef PROJECT_CLIENT
#define PROJECT_CLIENT

#define MAX_HOSTS  5

#include <netdb.h>            // struct addrinfo
#include "network.h"


ip6_memaddr allocate_rmem(struct sockaddr_in6 *targetIP);
ip6_memaddr *allocate_bulk_rmem(struct sockaddr_in6 *targetIP, uint64_t size);
ip6_memaddr_block allocate_uniform_rmem(struct sockaddr_in6 *target_ip, uint64_t size);
int write_rmem(struct sockaddr_in6 *targetIP, char *payload,  ip6_memaddr *remoteAddr);
int write_bulk_rmem(struct sockaddr_in6 *target_ip, char *payload, ip6_memaddr *remote_addrs, int num_addrs);
int write_uniform_rmem(struct sockaddr_in6 *target_ip, char *payload, ip6_memaddr_block remote_block);
int free_rmem(struct sockaddr_in6 *targetIP,  ip6_memaddr *remoteAddr);
int get_rmem(char *receiveBuffer, int length, struct sockaddr_in6 *targetIP, ip6_memaddr *remoteAddr);
int read_uniform_rmem(char *rx_buf, int length, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr);
int write_raid_mem(struct sockaddr_in6 *targetIP, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remoteAddrs, int needed);
int read_raid_mem(struct sockaddr_in6 *targetIP, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remoteAddrs, int needed);
struct in6_addr *gen_rdm_ip6_target();
struct in6_addr *gen_ip6_target();
void set_host_list(struct in6_addr *host_addrs, int num_hosts);


struct in6_addr *get_ip6_target(uint8_t index);
int numHosts();

#endif
