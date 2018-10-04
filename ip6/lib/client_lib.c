#include <string.h>           // strcpy, memset(), and memcpy()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <stdio.h>            // printf() and sprintf()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <limits.h>           // USHRT_MAX

#ifndef DEFAULT
#include <rte_ethdev.h>       // main DPDK library
#include <rte_malloc.h>       // rte_zmalloc_socket()
#endif

#include "utils.h"
#include "client_lib.h"
// Pointer to host list that gets set in config.c
// Kinda bad way to handle this, the only purpose is
// to acquire an IP target
// TODO: Fix
static struct in6_addr *hostList;
//
static int nhosts;
// This should be a hashmap (ideally using uthash.h)
// Contains unique IDs of servers that are addressed
// in a bulk request
// SubIDS are used to track current burst progress and
// are intended for congestion control later
// TODO: Fix this bloated pos
static __thread uint32_t sub_ids[USHRT_MAX];

struct in6_addr gen_rdm_ip6_target() {
    // Pick a random host
    uint8_t rndHost = rand() % nhosts;
    return hostList[rndHost];
}

struct in6_addr get_ip6_target(uint8_t index) {
    // Pick the host id at index
    return hostList[index];
}

// A getter function yay
int get_num_hosts() {
    return nhosts;
}

// A setter function yay
void set_host_list(struct in6_addr *host_addrs, int num_hosts) {
    hostList = host_addrs;
    nhosts = num_hosts;
}

// TODO: Implement meaningful return types and error messages
ip6_memaddr allocate_rmem(struct sockaddr_in6 *target_ip) {
    // Set the command of the destination IP
    ((ip6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_CMD;
    print_debug("******ALLOCATE******");
    // Send the request to the server, no data required, payload is NULL
    send_udp_raw(NULL, 0, (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port);
    // Create a new memory pointer
    ip6_memaddr retAddr;
    // Fill the the skeleton of the ip6 pointer (might no be necessary?)
    // Maybe replace with memset
    memcpy(&retAddr, &target_ip->sin6_addr, IPV6_SIZE);
    // retAddr gets modified in the rcv function,
    // this may need to be fixed to avoid confusion
    rcv_udp6_raw(NULL, target_ip, &retAddr);
    // Set the id with the server response (The packets dst addr)
    retAddr.subid = ((ip6_memaddr *)&target_ip->sin6_addr)->subid;
    return retAddr;
}

// TODO: Implement meaningful return types and error messages
ip6_memaddr *allocate_bulk_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks) {
    // Set the command of the destination IP
    ((ip6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_BULK_CMD;
    print_debug("******ALLOCATE BULK******");
    // Send the request to the server, data contains amount of pointers we request
    // Size is uint64_t because we are ridiculous
    send_udp_raw((uint8_t *)&num_blocks, sizeof(uint64_t), (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port);
    // Create a new memory pointer
    ip6_memaddr retAddr;
    // Fill the the skeleton of the ip6 pointer (might no be necessary?)
    // Maybe replace with memset
    memcpy(&retAddr, &target_ip->sin6_addr, IPV6_SIZE);
    // retAddr gets modified in the rcv function,
    // this may need to be fixed to avoid confusion
    rcv_udp6_raw(NULL, target_ip, &retAddr);
    // Set the id with the server response (The packets dst addr)
    retAddr.subid = ((ip6_memaddr *)&target_ip->sin6_addr)->subid;
    ip6_memaddr *addrList = malloc(num_blocks * sizeof(ip6_memaddr));
    // Convert the returned pointer into an array of pointers
    for (uint64_t i = 0; i < num_blocks; i++) {
        addrList[i] = retAddr;
        addrList[i].paddr = retAddr.paddr + i * BLOCK_SIZE;
    }
    return addrList;
}

// TODO: Implement meaningful return types and error messages
// Same idea as the other allocate functions except that we return a single ptr pointing to multiple addresses
ip6_memaddr_block allocate_uniform_rmem(struct sockaddr_in6 *target_ip, uint64_t num_blocks) {
    // Send the command to the target host and wait for response
    ((ip6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_BULK_CMD;
    print_debug("******ALLOCATE BULK******");
    send_udp_raw((uint8_t *)&num_blocks, sizeof(uint64_t), (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port);
    ip6_memaddr_block retAddr;
    retAddr.length = num_blocks;
    memcpy(&retAddr.memaddr, &target_ip->sin6_addr, IPV6_SIZE);
    rcv_udp6_raw(NULL, target_ip, &retAddr.memaddr);
    retAddr.memaddr.subid = ((ip6_memaddr *)&target_ip->sin6_addr)->subid;
    retAddr.offset = 0;
    return retAddr;
}

// TODO: Implement meaningful return types and error messages
int read_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint8_t *rx_buf, uint16_t length) {
    // Send the command to the target host and wait for response
    remote_addr->cmd =  READ_CMD;
    remote_addr->args = length;
    print_debug("******GET DATA******");
    // Send request and store response in the buffer
    send_udp_raw(NULL, 0, remote_addr, target_ip->sin6_port);
    int numBytes = rcv_udp6_raw(rx_buf, target_ip, NULL);
    return numBytes;
}

// TODO: Implement meaningful return types and error messages
void batch_read(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint8_t *buffer, uint16_t chunk_len, uint32_t num_packets){
    pkt_rqst pkts[num_packets];
    // Pointless operation, obsolete once the hashmap is done
    memset(sub_ids, 0, USHRT_MAX * sizeof(uint32_t));
    // List of pointers that will be copied to the pkt payload
    // The idea is that the server replies with the id of the ptr that is to be written to
    // Pretty neat solution actually since it uses the farm style write-mechanism
    // Might no need this one, TODO: fix
    uint64_t ptk_ptrs[num_packets];
    // I hate this but we need to create a bunch of packets to burst
    // Super inflexible and duplicated in write/trim functions
    for (uint32_t i = 0; i < num_packets; i++){
        pkts[i].dst_addr = remote_addrs[i];
        pkts[i].dst_port = target_ip->sin6_port;
        ptk_ptrs[i] = (uint64_t) &buffer[i*chunk_len];
        pkts[i].data = (uint8_t *) &ptk_ptrs[i]; // The server will write to this ptr
        pkts[i].datalen = POINTER_SIZE; // Just sending a ptr to the server
        pkts[i].dst_addr.args = sub_ids[remote_addrs[i].subid] + 1; // TODO: Fix!
        pkts[i].dst_addr.cmd =  READ_BULK_CMD;
        pkts[i].dst_addr.args = chunk_len; // We write in fixed size chunks
        sub_ids[remote_addrs[i].subid]++;
    }
    print_debug("******WRITE BULK DATA %d packets ******", num_packets);
    // Off we go
    send_udp_raw_batched(pkts, sub_ids, num_packets);
    // Kinda crappy receive function, makes lots of assumptions
    rcv_udp6_raw_bulk(num_packets);
}

int read_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, uint8_t *rx_buf, uint64_t size) {
    // Data fits into a single request, do not need that overloaded bulk operation
    if (size <= BLOCK_SIZE) {
        read_rmem(target_ip, remote_addrs, rx_buf, size);
    } else {
        uint64_t num_chunks = size / BLOCK_SIZE; // Amount of chunks we need to send
        uint16_t chunk_residual = size % BLOCK_SIZE; // Residual byte data
        if (unlikely(num_chunks > num_addrs)) // Simple check, should be an assert
            RETURN_ERROR(EXIT_FAILURE, "Not enough memory addresses provided!");
        uint32_t batches = num_chunks / BATCH_SIZE; // Next to chunks we have batches
        uint32_t batch_residual = num_chunks % BATCH_SIZE; // rest that is too small for a batch
        // Burst out all full batches
        for (uint32_t i = 0; i < batches; i++ )
            batch_read(target_ip, &remote_addrs[i*BATCH_SIZE], &rx_buf[BLOCK_SIZE*i*BATCH_SIZE], BLOCK_SIZE, BATCH_SIZE);
        // Burst out the incomplete batch
        uint64_t offset = num_chunks - batch_residual;
        batch_read(target_ip, &remote_addrs[offset], &rx_buf[BLOCK_SIZE*offset], BLOCK_SIZE, batch_residual);
        // Write out all leftover data that is smaller than BLOCK_SIZE
        if (chunk_residual)
           read_rmem(target_ip, remote_addrs, &rx_buf[BLOCK_SIZE*(offset+1)], chunk_residual);
    }
    return EXIT_SUCCESS;
}

// TODO: Implement meaningful return types and error messages
int read_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, uint8_t *rx_buf, uint64_t size) {
    // Generate a list of ptrs from a single given address
    // Not sure about this method...
    ip6_memaddr tmp_addrs[remote_block.length];
    for (uint64_t i = remote_block.offset; i < remote_block.length; i++) {
        tmp_addrs[i] = remote_block.memaddr;
        tmp_addrs[i].paddr = remote_block.memaddr.paddr + i * BLOCK_SIZE;
    }
    read_bulk_rmem(target_ip, &tmp_addrs[remote_block.offset], remote_block.length - remote_block.offset, rx_buf, size);
    return EXIT_SUCCESS;
}

// TODO: Implement meaningful return types and error messages
int write_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint8_t *payload, uint16_t length) {
    // Send the command to the target host and wait for response
    remote_addr->cmd =  WRITE_CMD;
    print_debug("******WRITE DATA******");
    // Send data
    send_udp_raw(payload, length, remote_addr, target_ip->sin6_port);
    // Block to avoid overloading the server
    rcv_udp6_raw(NULL, target_ip, NULL);
    return EXIT_SUCCESS;
}

// TODO: Implement meaningful return types and error messages
void batch_write(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint8_t *payload, uint16_t chunk_len, int num_packets){
    // Send the command to the target host and wait for response
    pkt_rqst pkts[num_packets];
    // Pointless operation, obsolete once the hashmap is done
    memset(sub_ids, 0, USHRT_MAX * sizeof(uint32_t));
    // I hate this but we need to create a bunch of packets to burst
    // Super inflexible and duplicated in write/trim functions
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
    // It can happen that we send to multiple serves
    // Thus we wait for a response from each unique server ID we collected in the marshaling process
    for (int i = 0; i< USHRT_MAX; i++) {
        if (sub_ids[i] != 0)
            rcv_udp6_raw(NULL, target_ip, NULL);
    }
}

// TODO: Implement meaningful return types and error messages
// Nearly identical to read_rmem
int write_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addrs, uint64_t num_addrs, uint8_t *payload, uint64_t size) {
    if (size <= BLOCK_SIZE) {
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

// TODO: Implement meaningful return types and error messages
int write_uniform_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr_block remote_block, uint8_t *payload, uint64_t size) {
    // Generate a list of ptrs from a single given address
    // Not sure about this method...
    ip6_memaddr tmp_addrs[remote_block.length];
    for (uint64_t i = remote_block.offset; i < remote_block.length; i++) {
        tmp_addrs[i] = remote_block.memaddr;
        tmp_addrs[i].paddr = remote_block.memaddr.paddr +i*BLOCK_SIZE;
    }
    write_bulk_rmem(target_ip, &tmp_addrs[remote_block.offset], remote_block.length - remote_block.offset, payload, size);
    return EXIT_SUCCESS;
}

// TODO: Implement meaningful return types and error messages
int free_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
    // Create message
    remote_addr->cmd = FREE_CMD;
    print_debug("******FREE DATA******");
    // We do not send data and we do not expect any data back
    // Very fast, simple operation
    send_udp_raw(NULL, 0, remote_addr, target_ip->sin6_port);
    rcv_udp6_raw(NULL, target_ip, remote_addr);
    return EXIT_SUCCESS;
}

// TODO: Implement meaningful return types and error messages
// TODO: Figure out how to merge trim with read and write
// Idea for trim is to act as a memset operation, zero all memory very quickly
// A memcpy of zeroes would be too slow
int trim_bulk_rmem(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr, uint64_t size) {
    // Create message
    remote_addr->cmd = TRIM_CMD;
    print_debug("******FREE DATA******");
    send_udp_raw((uint8_t *)&size, sizeof(uint64_t), remote_addr, target_ip->sin6_port);
    rcv_udp6_raw(NULL, target_ip, remote_addr);
    return EXIT_SUCCESS;
}

// DEPRECATED
//Returns the missing raid read -1 if everything is read
int read_raid_mem(struct sockaddr_in6 *target_ip, int hosts, uint8_t (*bufs)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remote_addrs, int needed) {
    int host;
    uint8_t rx_buf[BLOCK_SIZE];

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
        int bytes = rcv_udp6_raw(rx_buf, target_ip, NULL);
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
int write_raid_mem(struct sockaddr_in6 *target_ip, int hosts, uint8_t (*payload)[MAX_HOSTS][BLOCK_SIZE], ip6_memaddr **remote_addrs, int needed) {
    int host;
    uint8_t rx_buf[BLOCK_SIZE];

    for (int i=0; i <hosts;i++) {
        //printf("sending write request packet %d\n",i);
        remote_addrs[i]->cmd = WRITE_CMD;
        //printf("Sending Page UDP\n");
        send_udp_raw((uint8_t*)&((*payload)[i]), BLOCK_SIZE, remote_addrs[i], target_ip->sin6_port);
        //printf("FINISHED sending write request packet %d\n",i);
    }
    for (int i=0; i <hosts;i++) {
        //printf("receiving %d\n",i);
        //TODO check a list to ensure that the correct messages are
        //being acked
        //TODO timeout or something if a failure occurs here
        //printf("reading write ACK request packet %d\n",i);

        int bytes = rcv_udp6_raw(rx_buf, target_ip, NULL);
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
