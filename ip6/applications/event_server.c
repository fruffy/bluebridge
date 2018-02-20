#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/resource.h>
#include <errno.h>

static int NUM_THREADS = 1;

#include "../lib/network.h"
#include "../lib/utils.h"

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
#ifndef SOCK_RAW
    launch_server_loop(&myConf);
#endif
    return 0;
}
