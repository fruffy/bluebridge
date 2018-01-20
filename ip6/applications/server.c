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
void handle_requests(char *receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    // Switch on the client command
    if (remoteAddr->cmd == ALLOC_CMD) {
        print_debug("******ALLOCATE******");
        allocate_mem(targetIP);
    } else if (remoteAddr->cmd == WRITE_CMD) {
        print_debug("******WRITE DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        }
        write_mem(receiveBuffer, targetIP, remoteAddr);
    } else if (remoteAddr->cmd == GET_CMD) {
        print_debug("******GET DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        }
        get_mem(targetIP, remoteAddr);
    } else if (remoteAddr->cmd == FREE_CMD) {
        print_debug("******FREE DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        }
        free_mem(targetIP, remoteAddr);
    } else if (remoteAddr->cmd == ALLOC_BULK_CMD) {
        print_debug("******ALLOCATE BULK DATA: ");
        if (DEBUG) {
            print_n_bytes((char *) remoteAddr,IPV6_SIZE);
        }
        uint64_t *alloc_size = (uint64_t *) receiveBuffer;
        allocate_mem_bulk(targetIP, *alloc_size);
    } else {
        printf("Cannot match command %d!\n",remoteAddr->cmd);
        if (send_udp_raw("Hello, world!", 13, targetIP) == -1) {
            perror("ERROR writing to socket");
            exit(1);
        }
    }
}

typedef struct _thread_data_t {
  int tid;
} thread_data_t;
atomic_int round_robin = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;;
void *thread_loop(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    // Assign threads to cores
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    int assigned = data->tid % num_cores;
    pthread_t my_thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(assigned, &cpuset);
    pthread_setaffinity_np(my_thread, sizeof(cpu_set_t), &cpuset);
    printf("Assigned Thread %d to core %d\n", data->tid, assigned );
    
    // Initialize BlueBridge
    struct config myConf = get_bb_config();
    struct sockaddr_in6 *temp = init_net_thread(data->tid, &myConf, 1);
   // Start waiting for connections
    struct in6_memaddr remoteAddr;
    char receiveBuffer[BLOCK_SIZE];
    while (1) {
        rcv_udp6_raw(receiveBuffer, BLOCK_SIZE, temp, &remoteAddr);
        if (pthread_mutex_trylock(&mutex) == EBUSY)
            continue;
        handle_requests(receiveBuffer, temp, &remoteAddr);
        pthread_mutex_unlock(&mutex);
    }
    close_sockets();
    return NULL;
}
void threaded_server(){
    pthread_t thr[NUM_THREADS];
    int i;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];
    pthread_attr_t attr;
    size_t  stacksize = 0;

    pthread_attr_init( &attr );
    pthread_attr_getstacksize( &attr, &stacksize );
    printf("before stacksize : [%lu]\n", stacksize);
    pthread_attr_setstacksize( &attr, 99800000 );
    pthread_attr_getstacksize( &attr, &stacksize );
    printf("after  stacksize : [%lu]\n", stacksize);

    pthread_attr_init(&attr);

    if(pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0)
        fprintf(stderr, "Unable to set policy.\n");

    for (i = 0; i < NUM_THREADS; i++) {
        int rc;
        thr_data[i].tid =  i;
        struct rlimit limit;
        getrlimit (RLIMIT_STACK, &limit);
        printf ("\nStack Limit = %ld and %ld max\n", limit.rlim_cur, limit.rlim_max);
        printf("Launching thread %d\n", i );
        if ((rc = pthread_create(&thr[i],  &attr, thread_loop, &thr_data[i]))) {
          fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        }
    }
    /* block until all threads complete */
    for (i = 0; i < NUM_THREADS; i++) {
        printf("Thread %d Waiting for my friends...\n", i);
        pthread_join(thr[i], NULL);
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

    if (NUM_THREADS > 1) {
        threaded_server();
    } else {
       // Start waiting for connections
        struct in6_memaddr remoteAddr;
        char receiveBuffer[BLOCK_SIZE];
        while (1) {
            //TODO: Error handling (numbytes = -1)
            rcv_udp6_raw(receiveBuffer, BLOCK_SIZE, targetIP, &remoteAddr);
            handle_requests(receiveBuffer, targetIP, &remoteAddr);
        }
        close_sockets();
    }
    return 0;
}
