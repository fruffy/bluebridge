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

struct in6_addr *hostList;
int nhosts;

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
struct in6_memaddr allocate_rmem(struct sockaddr_in6 *target_ip) {
    // Send the command to the target host and wait for response
    ((struct in6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_CMD;
    print_debug("******ALLOCATE******");
    send_udp_raw(NULL, 0, (struct in6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port); 
    struct in6_memaddr retAddr;
    memcpy(&retAddr, &target_ip->sin6_addr, IPV6_SIZE);
    rcv_udp6_raw_id(NULL, 0, target_ip, &retAddr);
    retAddr.subid = ((struct in6_memaddr *)&target_ip->sin6_addr)->subid;
    return retAddr;
}

/*
 * Allocates a bulk of memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
struct in6_memaddr *allocate_rmem_bulk(struct sockaddr_in6 *target_ip, uint64_t size) {
    char tx_buf[BLOCK_SIZE];
    // Send the command to the target host and wait for response
    memcpy(tx_buf, &size, sizeof(uint64_t));
    ((struct in6_memaddr *)&target_ip->sin6_addr)->cmd = ALLOC_BULK_CMD;
    print_debug("******ALLOCATE BULK******");
    send_udp_raw(tx_buf, BLOCK_SIZE, (struct in6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port); 
    struct in6_memaddr retAddr;
    memcpy(&retAddr, &target_ip->sin6_addr, IPV6_SIZE);
    rcv_udp6_raw_id(NULL, 0, target_ip, &retAddr);
    retAddr.subid = ((struct in6_memaddr *)&target_ip->sin6_addr)->subid;
    struct in6_memaddr *addrList = malloc(size * sizeof(struct in6_memaddr));
    // Convert the returned pointer into an array of pointers
    for (uint64_t i = 0; i < size; i++) {
        addrList[i] = retAddr;
        addrList[i].paddr = retAddr.paddr + i * BLOCK_SIZE;
    }
    return addrList;
}

/*
 * Reads the remote memory based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int get_rmem(char *rx_buf, int length, struct sockaddr_in6 *target_ip, struct in6_memaddr *remote_addr) {
    // Send the command to the target host and wait for response
    remote_addr->cmd =  GET_CMD;
    print_debug("******GET DATA******");
    // Send request and store response
    send_udp_raw(NULL, 0, remote_addr, target_ip->sin6_port);
    int numBytes = rcv_udp6_raw_id(rx_buf, length, target_ip, remote_addr);
    return numBytes;
}

/*
 * Reads the remote memory based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int read_continuous_rmem(char *rx_buf, int length, struct sockaddr_in6 *target_ip, struct in6_memaddr *remote_addr) {
      int packets = length / BLOCK_SIZE;
      int residual = length % BLOCK_SIZE;
      for (int i = 0; i < packets; i++ ) {
        struct in6_memaddr tmp_addr = *remote_addr;
        tmp_addr.paddr = remote_addr->paddr +i*BLOCK_SIZE;
        get_rmem(&rx_buf[i*BLOCK_SIZE], length, target_ip, &tmp_addr);
      }
    struct in6_memaddr tmp_addr = *remote_addr;
    tmp_addr.paddr = remote_addr->paddr + packets * BLOCK_SIZE;
    get_rmem(&rx_buf[packets*BLOCK_SIZE], residual, target_ip, &tmp_addr);
    return length;
}

/*
 * Sends a write command to the server based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int write_rmem(struct sockaddr_in6 *target_ip, char *payload, struct in6_memaddr *remote_addr) {
    // Send the command to the target host and wait for response
    remote_addr->cmd =  WRITE_CMD;
    print_debug("******WRITE DATA******");
    send_udp_raw(payload, BLOCK_SIZE, remote_addr, target_ip->sin6_port);
    rcv_udp6_raw_id(NULL, 0, target_ip, remote_addr);
    return EXIT_SUCCESS;
}

/*
 * Sends a write command to the server based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
static __thread uint32_t sub_ids[USHRT_MAX];
static const int BATCH_SIZE = 50;

void batch_write(struct sockaddr_in6 *target_ip, char *payload, struct in6_memaddr *remote_addrs, int num_packets){
    // Send the command to the target host and wait for response
    struct pkt_rqst pkts[num_packets];
    memset(sub_ids, 0, USHRT_MAX * sizeof(uint32_t));
    for (int i = 0; i < num_packets; i++){
        pkts[i].dst_addr = remote_addrs[i];
        pkts[i].dst_port = target_ip->sin6_port;
        pkts[i].data = payload + BLOCK_SIZE * i;
        pkts[i].datalen = BLOCK_SIZE;
        pkts[i].dst_addr.args =sub_ids[remote_addrs[i].subid]+1; 
        pkts[i].dst_addr.cmd =  WRITE_BULK_CMD;
        sub_ids[remote_addrs[i].subid]++;
    }
    print_debug("******WRITE BULK DATA %d packets ******", num_packets );
    send_udp_raw_batched(pkts, sub_ids, num_packets);
    for (int i = 0; i< USHRT_MAX; i++) {
        if (sub_ids[i] != 0)
            rcv_udp6_raw_id(NULL, 0, target_ip, NULL);
    }
}

int write_rmem_bulk(struct sockaddr_in6 *target_ip, char *payload, struct in6_memaddr *remote_addrs, int num_addrs) {
    int batches = num_addrs / BATCH_SIZE;
    int batch_residual = num_addrs % BATCH_SIZE;
    for (int i = 0; i < batches; i++ ) {
        batch_write(target_ip, &payload[BLOCK_SIZE*i*BATCH_SIZE], &remote_addrs[i*BATCH_SIZE], BATCH_SIZE);
    }
    int offset = num_addrs - batch_residual;
    batch_write(target_ip, &payload[BLOCK_SIZE*offset], &remote_addrs[offset], batch_residual);
    return EXIT_SUCCESS;
}

/*
 * Releases the remote memory based on remote_addr
 */
// TODO: Implement meaningful return types and error messages
int free_rmem(struct sockaddr_in6 *target_ip,  struct in6_memaddr *remote_addr) {
    char tx_buf[BLOCK_SIZE];
    // Create message
    remote_addr->cmd =  FREE_CMD;
    print_debug("******FREE DATA******");
    // Send message and check if it was successful
    // TODO: Check if it actually was successful
    send_udp_raw(tx_buf, BLOCK_SIZE, remote_addr, target_ip->sin6_port);
    rcv_udp6_raw_id(NULL,0, target_ip, remote_addr);
    return EXIT_SUCCESS;
}




//Returns the missing raid read -1 if everything is read
int read_raid_mem(struct sockaddr_in6 *target_ip, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remote_addrs, int needed) {
    int host;
    char rx_buf[BLOCK_SIZE];

    int found[MAX_HOSTS];
    for (int i=0;i<hosts;i++) {
        found[i] = 0;
    }
    //memcpy(tx_buf + size, remote_addr, IPV6_SIZE);
    print_debug("******GET DATA******");
    for (int i=0; i<hosts;i++) {
        remote_addrs[i]->cmd =  GET_CMD;
        send_udp_raw(NULL, 0, remote_addrs[i], target_ip->sin6_port);
    }
    for (int i=0; i <hosts;i++) {
        //TODO check a list to ensure that the correct messages are
        //being acked
        //TODO timeout or something if a failure occurs here
        int bytes = rcv_udp6_raw_id(rx_buf, BLOCK_SIZE, target_ip, NULL);
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

int write_raid_mem(struct sockaddr_in6 *target_ip, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remote_addrs, int needed) {
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
        
        int bytes = rcv_udp6_raw_id(rx_buf, BLOCK_SIZE, target_ip, NULL);
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
