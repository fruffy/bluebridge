#define _GNU_SOURCE
#include "../lib/client_lib.h"
#include "../lib/utils.h"

#include <unistd.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/resource.h>
#include <pthread.h>
static int NUM_THREADS = 1;
static int NUM_ITERATIONS;

/////////////////////////////////// TO DOs ////////////////////////////////////
//  1. Check correctness of pointer on server side, it should never segfault.
//      (Ignore illegal operations)
//      -> Maintain list of allocated points
//      -> Should be very efficient
//      -> Judy array insert and delete or hashtable?
//  2. Implement userfaultd on the client side
//  3. We have nasty memory leaks that are extremely low level ()
//  4. Implement IP subnet state awareness
//      (server allocates memory address related to its assignment)
//  5. Remove unneeded code and print statements
//      Move all buffers to stack instead of heap.
//      Check memory leaks
///////////////////////////////////////////////////////////////////////////////

//To add the current correct route
//sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo
//ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dest=0:0:01ff:0:ffff:ffff:0:0,actions=output:2
struct LinkedPointer {
    struct in6_memaddr AddrString;
    struct LinkedPointer * Pointer;
};


void print_times(uint64_t* alloc_latency, uint64_t* read_latency, uint64_t* write_latency, uint64_t* free_latency, int length){
    /*FILE *allocF = fopen("alloc_latency.csv", "w");
    if (allocF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *readF = fopen("read_latency.csv", "w");
    if (readF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *writeF = fopen("write_latency.csv", "w");
    if (writeF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *freeF = fopen("free_latency.csv", "w");
    if (freeF == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    fprintf(allocF, "latency (ns)\n");
    fprintf(readF, "latency (ns)\n");
    fprintf(writeF, "latency (ns)\n");
    fprintf(freeF, "latency (ns)\n");

    int i;
    for (i = 0; i < NUM_ITERATIONS; i++) {
        fprintf(allocF, "%llu\n", (unsigned long long) alloc_latency[i]);
        fprintf(readF, "%llu\n", (unsigned long long) read_latency[i]);
        fprintf(writeF, "%llu\n", (unsigned long long) write_latency[i]);
        fprintf(freeF, "%llu\n", (unsigned long long) free_latency[i]);
    }

    fclose(allocF);
    fclose(readF);
    fclose(writeF);
    fclose(freeF);*/
    int i;
    uint64_t alloc_total = 0;
    uint64_t read_total = 0;
    uint64_t write_total = 0;
    uint64_t free_total = 0;

    for (i = 0; i < length; i++) {
        if (alloc_latency != NULL)
            alloc_total += (unsigned long long) alloc_latency[i];
        if (read_latency != NULL)
            read_total += (unsigned long long) read_latency[i];
        if (write_latency != NULL)
            write_total += (unsigned long long) write_latency[i];
        if (free_latency != NULL)
            free_total += (unsigned long long) free_latency[i];
    }

    if (alloc_latency != NULL)
        printf("Average allocate latency: "KRED"%lu us\n"RESET, alloc_total/ (length*1000));
    if (read_latency != NULL)
        printf("Average read latency: "KRED"%lu us\n"RESET, read_total/ (length*1000));
    if (write_latency != NULL)
        printf("Average write latency: "KRED"%lu us\n"RESET, write_total/ (length*1000));
    if (free_latency != NULL)
        printf("Average free latency: "KRED"%lu us\n"RESET, free_total/ (length*1000));

}

void basicOperations(struct sockaddr_in6 *targetIP) {
    uint64_t *alloc_latency = calloc(sizeof(uint64_t), NUM_ITERATIONS);
    uint64_t *read_latency = calloc(sizeof(uint64_t), NUM_ITERATIONS);
    uint64_t *write_latency = calloc(sizeof(uint64_t), NUM_ITERATIONS);
    uint64_t *free_latency = calloc(sizeof(uint64_t), NUM_ITERATIONS);

    struct config myConf = get_bb_config();
    struct in6_memaddr *r_addr = malloc(NUM_ITERATIONS * sizeof(struct in6_memaddr));
    if(!r_addr)
        perror("Allocation too large");

    int i;
    //struct in6_memaddr r_addr[NUM_ITERATIONS];
    srand(time(NULL));
    for (i = 0; i< NUM_ITERATIONS; i++) {
       // Generate a random IPv6 address out of a set of available hosts
        //memcpy(&(targetIP->sin6_addr), gen_rdm_IPv6Target(), sizeof(struct in6_addr));
        memcpy(&(targetIP->sin6_addr), gen_IPv6Target(i % myConf.num_hosts), sizeof(struct in6_addr));
        uint64_t start = getns(); 
        r_addr[i] = allocateRemoteMem(targetIP);
        alloc_latency[i] = getns() - start; 
    }

    for (i = 0; i < NUM_ITERATIONS; i++) {
        struct in6_memaddr remoteMemory = r_addr[i];
        //print_debug("Using Pointer: %p", (void *) getPointerFromIPv6(nextPointer->AddrString));
        print_debug("Creating payload");
        char *payload = malloc(4096);
        snprintf(payload, 50, "HELLO WORLD! How are you? %d", i);
        uint64_t wStart = getns();
        writeRemoteMem(targetIP, payload, &remoteMemory);
        write_latency[i - 1] = getns() - wStart;
        free(payload);
    }
    for (i = 0; i < NUM_ITERATIONS; i++) {
        struct in6_memaddr remoteMemory = r_addr[i];
        uint64_t rStart = getns();
        char *test = getRemoteMem(targetIP, &remoteMemory);
        read_latency[i - 1] = getns() - rStart;
        char *payload = malloc(4096);
        snprintf(payload, 50, "HELLO WORLD! How are you? %d", i);
        print_debug("Results of memory store: %.50s", test);
        if (strncmp(test, payload, 50) < 0) {
            print_debug(KRED"ERROR: WRONG RESULT"RESET);
            exit(1);
        }
        free(payload);
    }
    for (i = 0; i < NUM_ITERATIONS; i++) {
        struct in6_memaddr remoteMemory = r_addr[i];
        uint64_t fStart = getns();
        freeRemoteMem(targetIP, &remoteMemory);
        free_latency[i-1] = getns() - fStart;
    }
    print_times(alloc_latency, read_latency, write_latency, free_latency, NUM_ITERATIONS);
}



typedef struct _thread_data_t {
  int tid;
  struct in6_memaddr *r_addr;
  struct sockaddr_in6 *targetIP;
  int length;
} thread_data_t;
void *testing_loop(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    // Allocate measurement arrays
    uint64_t read_latency[data->length];
    uint64_t write_latency[data->length];
    uint64_t free_latency[data->length];

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
    struct sockaddr_in6 *temp = init_net_thread(data->tid, &myConf);
    
    // WRITE TEST
    for (int i = 0; i < data->length; i++) {
        struct in6_memaddr *remoteMemory = data->r_addr + i;
        //print_debug("Using Pointer: %p", (void *) getPointerFromIPv6(nextPointer->AddrString));
        print_debug("Creating payload");
        char payload[4096];
        snprintf(payload, 50, "HELLO WORLD! MY ID is: %d", data->tid);
        uint64_t wStart = getns();
        writeRemoteMem(temp, payload, remoteMemory);
        write_latency[i - 1] = getns() - wStart;
    }
    // GET TEST
    for (int i = 0; i < data->length; i++) {
        struct in6_memaddr *remoteMemory = data->r_addr + i;
        uint64_t rStart = getns();
        char *test = getRemoteMem(temp, remoteMemory);
        read_latency[i - 1] = getns() - rStart;
        print_debug("Thread: %d, Results of memory store: %s\n",  data->tid, test);
        char payload[4096];
        snprintf(payload, 50, "HELLO WORLD! MY ID is: %d", data->tid);
        if (strncmp(test,payload, 50) < 0) {
            print_debug(KRED"ERROR: WRONG RESULT"RESET);
            exit(1);
        }
    }
    // FREE TEST
    for (int i = 0; i < data->length; i++) {
        struct in6_memaddr *remoteMemory = data->r_addr + i;
        uint64_t fStart = getns();
        freeRemoteMem(temp, remoteMemory);
        free_latency[i-1] = getns() - fStart;
    }
    free(temp);
    printSendLat();
    print_times(NULL, read_latency, write_latency, free_latency, data->length);
    return NULL;
}

void basic_op_threads(struct sockaddr_in6 *targetIP) {
    
    pthread_t thr[NUM_THREADS];
    int i;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];

    uint64_t *alloc_latency = calloc(sizeof(uint64_t), NUM_ITERATIONS);

    struct in6_memaddr *r_addr = malloc(NUM_ITERATIONS * sizeof(struct in6_memaddr));
    if(!r_addr)
        perror("Allocation too large");
    
    //struct in6_memaddr r_addr[NUM_ITERATIONS];
    srand(time(NULL));
    for (i = 0; i< NUM_ITERATIONS; i++) {
       // Generate a random IPv6 address out of a set of available hosts
        memcpy(&(targetIP->sin6_addr), gen_rdm_IPv6Target(), sizeof(struct in6_addr));
        uint64_t start = getns(); 
        r_addr[i] = allocateRemoteMem(targetIP);
        alloc_latency[i] = getns() - start; 
    }
    pthread_attr_t attr;
    size_t  stacksize = 0;

    pthread_attr_init( &attr );
    pthread_attr_getstacksize( &attr, &stacksize );
    printf("before stacksize : [%lu]\n", stacksize);
    pthread_attr_setstacksize( &attr, 99800000 );
    pthread_attr_getstacksize( &attr, &stacksize );
    printf("after  stacksize : [%lu]\n", stacksize);
    close_sockets();
    //close_send_socket();
    //close_rcv_socket();
    /* create threads */
    for (i = 0; i < NUM_THREADS; i++) {
        int rc;
        int split = NUM_ITERATIONS/NUM_THREADS;
        thr_data[i].tid =  i;
        thr_data[i].targetIP =  targetIP;
        thr_data[i].r_addr =  &r_addr[split *i];
        thr_data[i].length = split;
        struct rlimit limit;

        getrlimit (RLIMIT_STACK, &limit);
        printf ("\nStack Limit = %ld and %ld max\n", limit.rlim_cur, limit.rlim_max);
        printf("Launching thread %d\n", i );
        if ((rc = pthread_create(&thr[i],  &attr, testing_loop, &thr_data[i]))) {
          fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        }
    }
    /* block until all threads complete */
    for (i = 0; i < NUM_THREADS; i++) {
        printf("Thread %d Waiting for my friends...\n", i);
        pthread_join(thr[i], NULL);
    }
    long alloc_total = 0;
    for (i = 0; i < NUM_ITERATIONS; i++) {
            alloc_total += (unsigned long long) alloc_latency[i];
    }
    printf("Average allocate latency: "KRED"%lu us\n"RESET, alloc_total/ (NUM_ITERATIONS*1000));
    free(alloc_latency);
}

/*
 * Main workhorse method. Parses arguments, setups connections
 * Allows user to issue commands on the command line.
 */
int main(int argc, char *argv[]) {
    // Example Call:
    //./applications/bin/testing -c ./tmp/config/distMem.cnf -t 4 -i 10000
    int c; 
    struct config myConf;
    while ((c = getopt (argc, argv, "c:i:t:")) != -1) { 
    switch (c) 
      { 
      case 'c':
        myConf = set_bb_config(optarg, 0);
        break;
      case 'i':
        NUM_ITERATIONS = atoi(optarg);
        break;
      case 't': 
        NUM_THREADS = atoi(optarg); 
        break;
      case '?': 
          fprintf (stderr, "Unknown option `-%c'.\n", optopt); 
        return 1; 
      default: 
        abort (); 
      } 
    } 
    printf("Running test with %d threads and %d iterations.\n", NUM_THREADS, NUM_ITERATIONS );
    struct sockaddr_in6 *temp = init_sockets(&myConf);
    set_host_list(myConf.hosts, myConf.num_hosts);

    struct timeval st, et;
    gettimeofday(&st,NULL);
    if (NUM_THREADS > 1)
        basic_op_threads(temp);
    else
        basicOperations(temp);
    gettimeofday(&et,NULL);
    int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec);
    printf("Total Time: %d micro seconds\n",elapsed);
    printf(KRED "Finished\n");
    printf(RESET);
    printSendLat();
    free(temp);
    //close_sockets();
    return 0;
}

