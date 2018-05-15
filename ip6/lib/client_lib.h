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
 * @param target_ip The address and port of the target server.
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
 * @param target_ip The address and port of the target server.
 * @param num_blocks Number of BLOCK_SIZE blocks we want to allocate.
 * 
 * @return An array of pointers single pointer which each point to a different byte range of size BLOCK_SIZE.
 */
ip6_memaddr *allocate_bulk_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks);
/**
 * @brief Allocates a continuous block of memory and returns the starting pointer.
 * @details This function is a more elegant implementation of the bulk_rmem allocation. Instead of
 * mallocing a number of pointers it returns a single starting pointer and the number of blocks 
 * associated with it. This saves space and is slightly more user friendly.
 * 
 * @param target_ip The address and port of the target server.
 * @param num_blocks Number of BLOCK_SIZE blocks we want to allocate.
 * 
 * @return An ip6_memaddr block containing the first pointer and the length of the continuous 
 * memory.
 */
ip6_memaddr_block allocate_uniform_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks);

/**
 * @brief Write to a single block of remote memory
 * @details This write operation is a simple single write of small size. Size must not exceed the 
 * max IP packet restrictions or MTU. In general, writes of size BLOCK_SIZE are recommended.
 * 
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_addr The target pointer which the server is supposed to write to.
 * @param payload Data
 * @param length Length of the data
 * @return This is a TODO, it should return appropriate errors.
 */
int write_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint8_t *payload, uint16_t length);
/**
 * @brief Write larger chunks of data to many pointers
 * @details This function takes a list of arbitrary ip6_memaddresses and write the given data set 
 * to it. It is important to note, that the amount of pointers has to match the size of the data. 
 * This function writes in batches of BATCH_SIZE and in blocks of BLOCK_SIZE.
 * If not, the function will throw an error. The advantage of this bulk function is that it can 
 * spread request across many machines instead of performing a large operation on a single server.
 * TODO: This entire function is not very elegant right now and quite complex. There are better 
 * ways to implement this type of write protocol. For example, it does not account for loss and 
 * uses a big array instead of a hash map. In general, a lot of items end up on the stack in this * function causing a large jump in latency.
 * 
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_addrs List of remote addresses the client is supposed to write to.
 * @param num_addrs Total number of addresses provided.
 * @param payload Data
 * @param size Size of the data. payload/size must be equal to num_addrs
 * @return This is a TODO, it should return appropriate errors.
 */
int write_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, uint8_t *payload, uint64_t size);
/**
 * @brief Write a continuous chunks of data to many pointers
 * @details This function takes a single ip6_memaddr and writes the given data set to it.
 * This function writes in batches of BATCH_SIZE and in blocks of BLOCK_SIZE.
 * As opposed to bulk_rmem() this function write to a continuous section of memory and does not
 * write on a per-pointer basis.
 * TODO: This entire function is not very elegant right now and quite complex. There are better 
 * ways to implement this type of write protocol. For example, it does not account for loss and 
 * uses a big array instead of a hash map. In general, a lot of items end up on the stack in this * function causing a large jump in latency.
 * 
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_block The starting pointer the client is supposed to write to.
 * @param payload Data
 * @param size Size of the data. payload/size must be equal to remote_block.length
 * @return This is a TODO, it should return appropriate errors.
 */
int write_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, uint8_t *payload, uint64_t size);

/**
 * @brief Read a single block of remote memory
 * @details This read operation is a simple single read of small size. Size must not exceed the 
 * max IP packet restrictions or MTU. In general, reads of size BLOCK_SIZE are recommended.
 * 
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_addr The target pointer which the client is supposed to read from.
 * @param rx_buf Buffer for data
 * @param length Length of the data
 * @return This is a TODO, it should return appropriate errors.
 */
int read_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint8_t *rx_buf, uint16_t length);
/**
 * @brief Read a large chunk of remote memory.
 * @details Similar to write_bulk_rmem() this function takes a list of pointers and retrieves data 
 * from them. It is important that the order of pointers matches up with the given buffer. In 
 * addition, size must not exceed the amount of available pointers times BLOCK_SIZE.
 * Otherwise, the receive buffer will be corrupted.
 * This function reads in batches of BATCH_SIZE and in blocks of BLOCK_SIZE. It is not elegant and 
 * can benefit from many improvements.
 * 
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_addrs The list of pointers which the client is supposed to read from.
 * @param num_addrs Total number of addresses provided.
 * @param rx_buf Buffer for data
 * @param size Size of the data. rx_buf/size must be equal to num_addrs
 * @return This is a TODO, it should return appropriate errors.
 */
int read_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, uint8_t *rx_buf, uint64_t size);

/**
 * @brief Read continuous chunks of data
 * @details This function takes a single ip6_memaddr and expands it to read data in.
 * This function reads in batches of BATCH_SIZE and in blocks of BLOCK_SIZE.
 * As opposed to read_bulk_rmem() this function reads from a continuous section of memory and does 
 * not read on a per-pointer basis.
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_block The starting pointer the client is supposed to read from.
 * @param rx_buf Buffer for data
 * @param size Size of the data. rx_buf/size must be equal to remote_block.length
 * @return This is a TODO, it should return appropriate errors.
 */
int read_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, uint8_t *rx_buf, uint64_t size);

/**
 * @brief Free pointer at given address
 * @details This is a free() proxy operation. It frees the allocated pointer on the server.
 * Specifying a size is not necessary as malloc on the server side maintains all the necessary
 * meta-information.
 * 
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_addr The starting pointer the server is supposed to free.
 * @return This is a TODO, it should return appropriate errors.
 */
int free_rmem(struct sockaddr_in6 *target_ip,  ip6_memaddr *remote_addr);

/**
 * @brief Set memory to zero at a given address
 * @details This sets memory to zero on the server size. The purpose of this function is a faster
 * alternative to write_rmem(). It is not fully implemented yet and only supports continuous
 * zeroing.
 * 
 * @param target_ip The IP wrapper which saves the server's src IP and port on a rcv call.
 * @param remote_addr The starting pointer the server is supposed to zero.
 * @param size Size of the data that is supposed to be freed.
 * @return This is a TODO, it should return appropriate errors.
 */
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
