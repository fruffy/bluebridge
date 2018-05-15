#ifndef PROJECT_CLIENT
#define PROJECT_CLIENT

#define MAX_HOSTS  5
#define BATCH_SIZE 100 // 100 is a nice and round number for our batch size...

#include "network.h"
#include "types.h"

ip6_memaddr allocate_rmem(struct sockaddr_in6 *target_ip);
ip6_memaddr *allocate_bulk_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks);
ip6_memaddr_block allocate_uniform_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks);

int write_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, char *payload, uint16_t length);
int write_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, char *payload, uint64_t size);
int write_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, char *payload, uint64_t size);

int read_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, char *rx_buf, uint16_t length);
int read_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, char *rx_buf, uint64_t size);
int read_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, char *rx_buf, uint64_t size);

int free_rmem(struct sockaddr_in6 *target_ip,  ip6_memaddr *remote_addr);
int trim_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint64_t size);
/**
 * @brief Generates a random IPv6 address
 * @details Generates a random IPv6 address target under specific constraints.
 * This is used by the client to address a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request.
 * @return struct in6_addr
 */
struct in6_addr *gen_rdm_ip6_target();

/**
 * @brief Get the IP address of a given host index.
 * @details Generates a fixed IPv6 address target 
 * This is used by the client to address a specific server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request.
 * 
 * @param index The host index
 * @return struct in6_addr
 */
struct in6_addr *get_ip6_target(uint8_t index);

/**
 * @brief Set the client host list
 * @details This sets the list of hosts available to the client. It is usually provided
 * by the config file.
 * @param host_addrs List of host addresses
 * @param num_hosts The total number of hosts
 */
void set_host_list(struct in6_addr *host_addrs, int num_hosts);

/**
 * @brief Return the current number of servers
 * @return Number of hosts
 */
int get_num_hosts();

/**
 * @brief Raid write operation DEPRECATED
 * @details
 * 
 * @param sockaddr_in6 [description]
 * @param hosts [description]
 * @param payload [description]
 * @param remoteAddrs [description]
 * @param needed [description]
 * @return [description]
 */
int write_raid_mem(struct sockaddr_in6 *target_ip, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remoteAddrs, int needed);
/**
 * @brief Raid read operation DEPRECATED
 * @details [long description]
 * 
 * @param sockaddr_in6 [description]
 * @param hosts [description]
 * @param bufs [description]
 * @param remoteAddrs [description]
 * @param needed [description]
 * @return [description]
 */
int read_raid_mem(struct sockaddr_in6 *target_ip, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remoteAddrs, int needed);

#endif
