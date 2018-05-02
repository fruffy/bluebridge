#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/resource.h>
#include <stdatomic.h>
#include<errno.h>

static int NUM_THREADS = 1;

#include "../lib/server_lib.h"
#include "../lib/utils.h"

/*
 * Request handler for socket sock_fd
 * TODO: get message format
 */
void handle_requests(char *receiveBuffer, int size, struct sockaddr_in6 *target_ip, struct in6_memaddr *remoteAddr) {
    // Switch on the client command
    if (remoteAddr->cmd == ALLOC_CMD) {
        print_debug("******ALLOCATE******");
        allocate_mem(target_ip);
    } else if (remoteAddr->cmd == WRITE_CMD) {
        print_debug("******WRITE DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        }
        write_mem(receiveBuffer, target_ip, remoteAddr);
    } else if (remoteAddr->cmd == GET_CMD) {
        print_debug("******GET DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        }
        get_mem(target_ip, remoteAddr);
    } else if (remoteAddr->cmd == FREE_CMD) {
        print_debug("******FREE DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        }
        free_mem(target_ip, remoteAddr);
    } else if (remoteAddr->cmd == ALLOC_BULK_CMD) {
        print_debug("******ALLOCATE BULK DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr,IPV6_SIZE);
        }
        uint64_t *alloc_size = (uint64_t *) receiveBuffer;
        allocate_mem_bulk(target_ip, *alloc_size);
    } else {
        printf("Cannot match command %d!\n",remoteAddr->cmd);
        if (send_udp_raw("Hello, world!", 13, (struct in6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port) == -1) {
            perror("ERROR writing to socket");
            exit(1);
        }
    }
}

/*
 * Main workhorse method. Parses command args and does setup.
 * Blocks waiting for connections.
 */
int main(int argc, char *argv[]) {
    
    // Example Call: ./applications/bin/server -c tmp/config/distMem.cnf
    int c; 
    struct config myConf;
    while ((c = getopt (argc, argv, "c:i:t:")) != -1) { 
    switch (c) 
      { 
      case 'c':
        myConf = set_bb_config(optarg, 1);
        break;
      case 't': 
        NUM_THREADS = atoi(optarg); 
        break;
      case '?': 
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          printf("usage: -c config -t num_threads -i num_iterations>\n");
        return 1; 
      default: 
        abort (); 
      } 
    }
    printf("Running server with %d threads \n", NUM_THREADS );

    struct sockaddr_in6 *target_ip = init_sockets(&myConf, 1);
   // Start waiting for connections
    struct in6_memaddr remoteAddr;
    char receiveBuffer[BLOCK_SIZE];
    while (1) {
        //TODO: Error handling (numbytes = -1)
        int size = rcv_udp6_raw(receiveBuffer, BLOCK_SIZE, target_ip, &remoteAddr);
        handle_requests(receiveBuffer, size, target_ip, &remoteAddr);
    }
    close_sockets();
    return 0;
}
