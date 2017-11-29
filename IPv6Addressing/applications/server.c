#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/resource.h>

static int NUM_THREADS = 1;

#include "../lib/server_lib.h"
#include "../lib/utils.h"

/*
 * Request handler for socket sock_fd
 * TODO: get message format
 */
void handleClientRequests(char *receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    char *splitResponse;
    // Switch on the client command
    if (remoteAddr->cmd == ALLOC_CMD) {
        print_debug("******ALLOCATE******");
        allocateMem(targetIP);
    } else if (remoteAddr->cmd == WRITE_CMD) {
        print_debug("******WRITE DATA: ");
        if (DEBUG) {
            printNBytes((char *) remoteAddr, IPV6_SIZE);
        }
        writeMem(receiveBuffer, targetIP, remoteAddr);
    } else if (remoteAddr->cmd == GET_CMD) {
        print_debug("******GET DATA: ");
        // printNBytes((char *) ipv6Pointer,IPV6_SIZE);
        getMem(targetIP, remoteAddr);
    } else if (remoteAddr->cmd == FREE_CMD) {
        print_debug("******FREE DATA: ");
        if (DEBUG) {
            printNBytes((char *) remoteAddr,IPV6_SIZE);
        }
        freeMem(targetIP, remoteAddr);
    } else {
        printf("Cannot match command!\n");
        if (send_udp_raw("Hello, world!", 13, targetIP) == -1) {
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

    struct sockaddr_in6 *targetIP = init_sockets(&myConf);

   // Start waiting for connections
    struct in6_memaddr remoteAddr;
    char receiveBuffer[BLOCK_SIZE];
    while (1) {
        //TODO: Error handling (numbytes = -1)
        rcv_udp6_raw(receiveBuffer, BLOCK_SIZE, targetIP, &remoteAddr);
        handleClientRequests(receiveBuffer, targetIP, &remoteAddr);
    }
    close_sockets();
    return 0;
}
