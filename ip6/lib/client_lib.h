#ifndef PROJECT_CLIENT
#define PROJECT_CLIENT

#define MAX_HOSTS  5
#define BATCH_SIZE 100 // 100 is a nice and round number for our batch size...

#include "network.h"
#include "types.h"
/**
 * @brief Allocate pointer of BLOCK_SIZE
 * @details Allocates a single pointer of size BLOCK_SIZE on a remote server. 
 * The server will reply with a pointer to the allocated block which is converted 
 * into a 128 bit ip6_memaddr.
 * 
 * @param sockaddr_in6 The address and port of the target server.
 * @return A single pointer which points to a byte range of size BLOCK_SIZE.
 */
ip6_memaddr allocate_rmem(struct sockaddr_in6 *target_ip);

/**
 * @brief Allocate multiple blocks on one single server.
 * @details This function is an extension of allocate_mem. It allows the specification of
 * the number of blocks to allocate. The server still replies with a single pointer which is 
 * reconstructed into an appropriate number of remote memory pointers. The array has to be freed
 * after it has been used.
 * TODO: This function could potentially distribute pointers across multiple servers without the
 * client explicitly specifying destination servers.
 * 
 * @param sockaddr_in6 The address and port of the target server.
 * @param num_blocks Number of BLOCK_SIZE blocks we want to allocate.
 * 
 * @return An array of pointers single pointer which each point to a different byte range of size BLOCK_SIZE.
 */
ip6_memaddr *allocate_bulk_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks);
/**
 * @brief Allocates a continuous block of memory and returns the starting pointer.
 * @details This function is a more elegant implementation of the bulk_rmem allocation. Instead of
 * mallocing a number of pointers it returns a single starting pointer and the number of blocks 
 * associated with it. This saves space and is slightly more userfriendly.
 * 
 * @param sockaddr_in6 The address and port of the target server.
 * @param num_blocks Number of BLOCK_SIZE blocks we want to allocate.
 * 
 * @return An ip6_memaddr block containing the first pointer and the length of the continuous 
 * memory.
 */
ip6_memaddr_block allocate_uniform_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks);

/**
 * @brief Simple single block write
 * @details This write operation is a simple single write of small size. Size must not exceed the 
 * max IP packet restrictions or MTU. In general, writes of size BLOCK_SIZE are recommended.
 * 
 * @param sockaddr_in6 The IP wrapper which 
 * @param remote_addr The target pointer that is being written to. 
 * @param payload 
 * @param length [description]
 * @return [description]
 */
int write_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint8_t *payload, uint16_t length);
int write_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, uint8_t *payload, uint64_t size);
int write_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, uint8_t *payload, uint64_t size);

int read_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint8_t *rx_buf, uint16_t length);
int read_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, uint8_t *rx_buf, uint64_t size);
int read_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, uint8_t *rx_buf, uint64_t size);

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
int write_raid_mem(struct sockaddr_in6 *target_ip, int hosts, uint8_t (*payload)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remoteAddrs, int needed);
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
int read_raid_mem(struct sockaddr_in6 *target_ip, int hosts, uint8_t (*bufs)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remoteAddrs, int needed);

#endif
