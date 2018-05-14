/*
 ** client_lib.c -- 
 */

// TODO: Add a get remote machine method (needs to get the remote machine which holds a specific address.)
#include <string.h>           // strcpy, memset(), and memcpy()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <stdio.h>            // printf() and sprintf()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <limits.h>           // USHRT_MAX

#include "client_lib.h"
#include "utils.h"

#ifndef DEFAULT
#include <rte_ethdev.h>       // main DPDK library
#include <rte_malloc.h>       // rte_zmalloc_socket()
#endif

#define BATCH_SIZE 100 // For some reason this is the maximum batch size we can do...

struct in6_addr *hostList;
int nhosts;
static __thread uint32_t sub_ids[USHRT_MAX]; // This should be a hashmap, right now it's just an array 

/*
 * Generates a random IPv6 address target under specific constraints
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr *gen_rdm_ip6_target() {
    // Pick a random host
    uint8_t rndHost = rand()% nhosts;
    return &hostList[rndHost];
}

/*Returns the IP of a given host*/
struct in6_addr *get_ip6_target(uint8_t index) {
    return &hostList[index];
}

//return the number of hosts
int numHosts() {
    return nhosts;
}

/*
 * Generates a random IPv6 address target under specific constraints
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr *gen_ip6_target(int offset) {
    // Pick a random host
    return &hostList[offset];
}

/*
 * Set the list of target hosts and their IP-adresses in our system.
 * Will be replaced by in-switch functionality.
 */
void set_host_list(struct in6_addr *host_addrs, int num_hosts) {
    hostList = host_addrs;
    nhosts = num_hosts;
}

/*
 * Allocate memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
ip6_memaddr allocate_rmem(struct sockaddr_in6 *target_ip) {
    // Send the command to the target host and wait for response
    ((ip6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_CMD;
    print_debug("******ALLOCATE******");
    send_udp_raw(NULL, 0, (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port); 
    ip6_memaddr retAddr;
    memcpy(&retAddr, &target_ip->sin6_addr, IPV6_SIZE);
    rcv_udp6_raw_id(NULL, target_ip, &retAddr);
    retAddr.subid = ((ip6_memaddr *)&target_ip->sin6_addr)->subid;
    return retAddr;
}

/*
 * Allocates a bulk of memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
ip6_memaddr *allocate_bulk_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks) {
    // Send the command to the target host and wait for response
    ((ip6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_BULK_CMD;
    print_debug("******ALLOCATE BULK******");
    send_udp_raw((char *)&num_blocks, sizeof(uint64_t), (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port); 
    ip6_memaddr retAddr;
    memcpy(&retAddr, &target_ip->sin6_addr, IPV6_SIZE);
    rcv_udp6_raw_id(NULL, target_ip, &retAddr);
    retAddr.subid = ((ip6_memaddr *)&target_ip->sin6_addr)->subid;
    ip6_memaddr *addrList = malloc(num_blocks * sizeof(ip6_memaddr));
    // Convert the returned pointer into an array of pointers
    for (uint64_t i = 0; i < num_blocks; i++) {
        addrList[i] = retAddr;
        addrList[i].paddr = retAddr.paddr + i * BLOCK_SIZE;
    }
    return addrList;
}

/*
 * Allocates a bulk of memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
ip6_memaddr_block allocate_uniform_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks) {
    // Send the command to the target host and wait for response
    ((ip6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_BULK_CMD;
    print_debug("******ALLOCATE BULK******");
    send_udp_raw((char *)&num_blocks, sizeof(uint64_t), (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port); 
    ip6_memaddr_block retAddr;
    retAddr.length = num_blocks;
    memcpy(&retAddr.memaddr, &target_ip->sin6_addr, IPV6_SIZE);
    rcv_udp6_raw_id(NULL, target_ip, &retAddr.memaddr);
    retAddr.memaddr.subid = ((ip6_memaddr *)&target_ip->sin6_addr)->subid;
    return retAddr;
}

/*
 * Reads the remote memory based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int read_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, char *rx_buf, uint16_t length) {
    // Send the command to the target host and wait for response
    remote_addr->cmd =  READ_CMD;
    remote_addr->args = length;
    print_debug("******GET DATA******");
    // Send request and store response
    send_udp_raw(NULL, 0, remote_addr, target_ip->sin6_port);
    int numBytes = rcv_udp6_raw_id(rx_buf, target_ip, NULL);
    return numBytes;
}

/*
 * Reads the remote memory based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
void batch_read(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, char *buffer, uint16_t chunk_len, uint32_t num_packets){
    pkt_rqst pkts[num_packets];
    memset(sub_ids, 0, USHRT_MAX * sizeof(uint32_t));
    uint64_t ptrs[num_packets];
    for (uint32_t i = 0; i < num_packets; i++){
        pkts[i].dst_addr = remote_addrs[i];
        pkts[i].dst_port = target_ip->sin6_port;
        ptrs[i] = (uint64_t) &buffer[i*chunk_len];
        pkts[i].data = (char *) &ptrs[i];
        pkts[i].datalen = POINTER_SIZE;
        pkts[i].dst_addr.args = sub_ids[remote_addrs[i].subid] + 1; 
        pkts[i].dst_addr.cmd =  READ_BULK_CMD;
        pkts[i].dst_addr.args = chunk_len;
        sub_ids[remote_addrs[i].subid]++;
    }
    print_debug("******WRITE BULK DATA %d packets ******", num_packets );
    send_udp_raw_batched(pkts, sub_ids, num_packets);
    rcv_udp6_raw_bulk(num_packets);
}

int read_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, char *rx_buf, uint64_t size) {
    if (size < BLOCK_SIZE) {
        read_rmem(target_ip, remote_addrs, rx_buf, size);
    } else {
        uint64_t num_chunks = size / BLOCK_SIZE;
        uint16_t chunk_residual = size % BLOCK_SIZE;
        if (unlikely(num_chunks > num_addrs))
            RETURN_ERROR(EXIT_FAILURE, "Not enough memory addresses provided!");
        uint32_t batches = num_chunks / BATCH_SIZE;
        uint32_t batch_residual = num_chunks % BATCH_SIZE;
        for (uint32_t i = 0; i < batches; i++ ) {
            batch_read(target_ip, &remote_addrs[i*BATCH_SIZE], &rx_buf[BLOCK_SIZE*i*BATCH_SIZE], BLOCK_SIZE, BATCH_SIZE);
        }
        uint64_t offset = num_chunks - batch_residual;
        batch_read(target_ip, &remote_addrs[offset], &rx_buf[BLOCK_SIZE*offset], BLOCK_SIZE, batch_residual);
        if (chunk_residual)
           read_rmem(target_ip, remote_addrs, &rx_buf[BLOCK_SIZE*(offset+1)], chunk_residual);
    }
    return EXIT_SUCCESS;
}


/*
 * Reads the remote memory based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int read_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, char *rx_buf, uint64_t size) {
    ip6_memaddr tmp_addrs[remote_block.length];
    for (uint64_t i = 0; i < remote_block.length; i++) {
        tmp_addrs[i] = remote_block.memaddr;
        tmp_addrs[i].paddr = remote_block.memaddr.paddr +i*BLOCK_SIZE;
    }
    read_bulk_rmem(target_ip, tmp_addrs, remote_block.length, rx_buf, size);
    return EXIT_SUCCESS;
}

/*
 * Sends a write command to the server based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int write_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, char *payload, uint16_t length) {
    // Send the command to the target host and wait for response
    remote_addr->cmd =  WRITE_CMD;
    print_debug("******WRITE DATA******");
    send_udp_raw(payload, length, remote_addr, target_ip->sin6_port);
    rcv_udp6_raw_id(NULL, target_ip, NULL);
    return EXIT_SUCCESS;
}

void batch_write(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, char *payload, uint16_t chunk_len, int num_packets){
    // Send the command to the target host and wait for response
    pkt_rqst pkts[num_packets];
    memset(sub_ids, 0, USHRT_MAX * sizeof(uint32_t));
    for (int i = 0; i < num_packets; i++){
        pkts[i].dst_addr = remote_addrs[i];
        pkts[i].dst_port = target_ip->sin6_port;
        pkts[i].data = payload + chunk_len * i;
        pkts[i].datalen = chunk_len;
        pkts[i].dst_addr.args =sub_ids[remote_addrs[i].subid] + 1; 
        pkts[i].dst_addr.cmd =  WRITE_BULK_CMD;
        sub_ids[remote_addrs[i].subid]++;
    }
    print_debug("******WRITE BULK DATA %d packets ******", num_packets );
    send_udp_raw_batched(pkts, sub_ids, num_packets);
    for (int i = 0; i< USHRT_MAX; i++) {
        if (sub_ids[i] != 0)
            rcv_udp6_raw_id(NULL, target_ip, NULL);
    }
}

int write_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, char *payload, uint64_t size) {
    if (size < BLOCK_SIZE) {
        write_rmem(target_ip, remote_addrs, payload, size);
    } else {
        uint64_t num_chunks = size / BLOCK_SIZE;
        uint16_t chunk_residual = size % BLOCK_SIZE;
        if (unlikely(num_chunks > num_addrs))
            RETURN_ERROR(EXIT_FAILURE, "Not enough memory addresses provided!");
        uint64_t batches = num_chunks / BATCH_SIZE;
        uint16_t batch_residual = num_chunks % BATCH_SIZE;
        for (uint64_t i = 0; i < batches; i++)
            batch_write(target_ip, &remote_addrs[i*BATCH_SIZE], &payload[BLOCK_SIZE*i*BATCH_SIZE], BLOCK_SIZE, BATCH_SIZE);
        uint64_t offset = num_chunks - batch_residual;
        batch_write(target_ip, &remote_addrs[offset], &payload[BLOCK_SIZE*offset], BLOCK_SIZE, batch_residual);
        if (chunk_residual)
            write_rmem(target_ip, remote_addrs, &payload[BLOCK_SIZE*(offset+1)], chunk_residual);
    }
    return EXIT_SUCCESS;
}

int write_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, char *payload, uint64_t size) {
    ip6_memaddr tmp_addrs[remote_block.length];
    for (uint64_t i = 0; i < remote_block.length; i++) {
        tmp_addrs[i] = remote_block.memaddr;
        tmp_addrs[i].paddr = remote_block.memaddr.paddr + i * BLOCK_SIZE;
    }
    write_bulk_rmem(target_ip, tmp_addrs, remote_block.length, payload, size);
    return EXIT_SUCCESS;
}

/*
 * Releases the remote memory based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int free_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
    // Create message
    remote_addr->cmd = FREE_CMD;
    print_debug("******FREE DATA******");
    // Send message and check if it was successful
    // TODO: Check if it actually was successful
    send_udp_raw(NULL, 0, remote_addr, target_ip->sin6_port);
    rcv_udp6_raw_id(NULL, target_ip, remote_addr);
    return EXIT_SUCCESS;
}

// DEPRECATED
//Returns the missing raid read -1 if everything is read
int read_raid_mem(struct sockaddr_in6 *target_ip, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remote_addrs, int needed) {
    int host;
    char rx_buf[BLOCK_SIZE];

    int found[MAX_HOSTS];
    for (int i=0;i<hosts;i++) {
        found[i] = 0;
    }
    //memcpy(tx_buf + size, remote_addr, IPV6_SIZE);
    print_debug("******GET DATA******");
    for (int i=0; i<hosts;i++) {
        remote_addrs[i]->cmd =  READ_CMD;
        send_udp_raw(NULL, 0, remote_addrs[i], target_ip->sin6_port);
    }
    for (int i=0; i <hosts;i++) {
        //TODO check a list to ensure that the correct messages are
        //being acked
        //TODO timeout or something if a failure occurs here
        int bytes = rcv_udp6_raw_id(rx_buf, target_ip, NULL);
        //take care of timeout
        if ( bytes == -1 && i >= needed) {
            //There is no point in reading another page if the timeout
            //occured, just break
            break;
        } else if (bytes == -1 && i < needed) {
            //In this case a timeout occured, but there is
            //insufficient data to repair a page, so another read must
            //be done.
            //TODO do retransmission of the request to the server.
            i--;
            continue;
        } else {
            //Here a page was actually read
            host = (int)target_ip->sin6_addr.s6_addr[5]-2;
            //printf("%d\n",host);
            //printf("rec :%s\n",rx_buf);
            //printf("Read From %d\n",(int)target_ip->sin6_addr.s6_addr[5]);
            memcpy(&((*bufs)[host]),rx_buf,BLOCK_SIZE);
            //printf("cpy :%s\n",(*bufs)[host]);
            //printf("copied\n");
            //printf("%d ",host);
            found[host] = 1;
        }
    }
    for (int i=0;i<hosts;i++) {
        if (found[i] == 0) {
            return i;
        }
    }
    return -1;
}

// DEPRECATED
int write_raid_mem(struct sockaddr_in6 *target_ip, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remote_addrs, int needed) {
    int host;
    char rx_buf[BLOCK_SIZE];

    for (int i=0; i <hosts;i++) {
        //printf("sending write request packet %d\n",i);
        remote_addrs[i]->cmd = WRITE_CMD;
        //printf("Sending Page UDP\n");
        send_udp_raw((char*)&((*payload)[i]), BLOCK_SIZE, remote_addrs[i], target_ip->sin6_port);
        //printf("FINISHED sending write request packet %d\n",i);
    }
    for (int i=0; i <hosts;i++) {
        //printf("receiving %d\n",i);
        //TODO check a list to ensure that the correct messages are
        //being acked
        //TODO timeout or something if a failure occurs here
        //printf("reading write ACK request packet %d\n",i);
        
        int bytes = rcv_udp6_raw_id(rx_buf, target_ip, NULL);
        if ( bytes == -1 && i >= needed) {
            //There is no point in reading another page if the timeout
            //occured, just break
            break;
        } else if (bytes == -1 && i < needed) {
            //In this case a timeout occured, but there is
            //insufficient data to repair a page, so another read must
            //be done.
            //TODO do retransmission of the request to the server.
            i--;
            continue;
        } else {
            host = (int)target_ip->sin6_addr.s6_addr[5];
            //printf("Read From %d\n",(int)target_ip->sin6_addr.s6_addr[5]);
            memcpy(&((*payload)[host-2]),rx_buf,BLOCK_SIZE);
        }
    }
    return EXIT_SUCCESS;
}
