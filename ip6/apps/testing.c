#define _GNU_SOURCE
#include "../lib/client_lib.h"
#include "../lib/utils.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/resource.h>
#include <pthread.h>
#include <sys/stat.h>

static int NUM_THREADS = 1;
static int BATCHED_MODE = 0;
static int BATCH_SIZE = 40;
static uint64_t NUM_ITERATIONS;



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

void create_dir(const char *name) {
    struct stat st = {0};
    if (stat(name, &st) == -1) {
        int MAX_FNAME = 256;
        char fname[MAX_FNAME];
        snprintf(fname, MAX_FNAME, "mkdir -p %s -m 777", name);
        printf("Creating dir %s...\n", name);
        system(fname);
    }
}

void save_time(const char *type, uint64_t* latency, uint64_t length){
    int MAX_FNAME = 256;
    char fname[MAX_FNAME];
    snprintf(fname, MAX_FNAME, "results/testing/t%d_iter%lu", NUM_THREADS, NUM_ITERATIONS);
    create_dir(fname);
    memset(fname, 0, MAX_FNAME);
    snprintf(fname, MAX_FNAME, "results/testing/t%d_iter%lu/%s.csv", NUM_THREADS, NUM_ITERATIONS, type);
    FILE *file = fopen(fname, "w");
    if (file == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }
    fprintf(file, "latency (ns)\n");
    uint64_t i;
    for (i = 0; i < length; i++) {
        fprintf(file, "%llu\n", (unsigned long long) latency[i]);
    }
    fclose(file);
    uint64_t total = 0;
    for (i = 0; i < length; i++)
        total += (unsigned long long) latency[i];
    printf("Average %s latency: "KRED"%lu us\n"RESET, type, total / (length*1000));
}

typedef struct _thread_data_t {
  int tid;
  struct sockaddr_in6 *target_ip;
  struct in6_memaddr *r_addr; 
  uint64_t length;
} thread_data_t;


void *testing_loop(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    // Allocate measurement arrays
    uint64_t alloc_latency[data->length];
    uint64_t read_latency[data->length];
    uint64_t read_latency2[data->length];
    uint64_t write_latency[data->length];
    uint64_t write_latency2[data->length];
    uint64_t free_latency[data->length];

/*    // Assign threads to cores
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    int assigned = data->tid % num_cores;
    pthread_t my_thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(assigned, &cpuset);
    pthread_setaffinity_np(my_thread, sizeof(cpu_set_t), &cpuset);
    printf("Assigned Thread %d to core %d\n", data->tid, assigned );*/
    // Initialize BlueBridge
    struct config myConf = get_bb_config();
    struct sockaddr_in6 *target_ip = init_net_thread(data->tid, &myConf, 0);
    struct in6_memaddr *r_addr = calloc(data->length,sizeof(struct in6_memaddr)); 
    if(!r_addr) 
        perror("Allocation too large"); 
 
    // ALLOC TEST
    printf("Thread %d Allocating...\n", data->tid);
    uint64_t aStart = getns();
    struct in6_addr *ipv6Pointer = gen_ip6_target(data->tid % myConf.num_hosts);
    memcpy(&(target_ip->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
    struct in6_memaddr *temp = allocate_rmem_bulk(target_ip, data->length);
    memcpy(r_addr, temp, data->length * sizeof(struct in6_memaddr));
    //free(temp);
    alloc_latency[0] = getns() - aStart;
    // WRITE TEST
    printf("Thread %d Starting write test...\n", data->tid);
    if (BATCHED_MODE) {
        for (uint64_t i = 0; i < data->length/BATCH_SIZE; i++) {
            char payload[BLOCK_SIZE * BATCH_SIZE];
            print_debug("Creating payload");
            for (int j = 0; j < BATCH_SIZE; j++) {
                print_debug("Writing %lu to offset %d\n", i*BATCH_SIZE + j, BLOCK_SIZE *j );
                snprintf(&payload[BLOCK_SIZE *(j)], 50, "HELLO WORLD! How are you? %lu", i*BATCH_SIZE + j);
            }
            uint64_t wStart = getns();
            write_rmem_bulk(target_ip, payload, (struct in6_memaddr *)&r_addr[i * BATCH_SIZE], BATCH_SIZE);
            write_latency[i] = getns() - wStart;
        }
    } else {
        for (uint64_t i = 0; i < data->length; i++) {
            struct in6_memaddr remoteMemory = r_addr[i];
            print_debug("Creating payload");
            char payload[BLOCK_SIZE];
            snprintf(payload, 50, "HELLO WORLD! How are you? %lu", i);
            uint64_t wStart = getns();
            write_rmem(target_ip, payload, &remoteMemory);
            write_latency[i] = getns() - wStart;
        }
    }
    char test[BLOCK_SIZE];
    // READ TEST
    printf("Thread %d Starting read test...\n", data->tid);
    for (uint64_t i = 0; i < data->length; i++) {
        struct in6_memaddr remoteMemory = r_addr[i];
        uint64_t rStart = getns();
        get_rmem(test, BLOCK_SIZE, target_ip, &remoteMemory);
        read_latency[i] = getns() - rStart;
        char payload[BLOCK_SIZE];
        snprintf(payload, 50, "HELLO WORLD! How are you? %lu", i);
        print_debug("Results of memory store: %.50s", test);
        if (strncmp(test, payload, 50) < 0) {
            perror(KRED"ERROR: WRONG RESULT"RESET);
            printf("Expected %s Got %s\n", payload, test );
            exit(1);
        }
    }
    // WRITE TEST 2
    printf("Thread %d Starting second write test...\n", data->tid);
    if (BATCHED_MODE) {
        for (uint64_t i = 0; i < data->length/BATCH_SIZE; i++) {
            char payload[BLOCK_SIZE * BATCH_SIZE];
            print_debug("Creating payload");
            for (int j = 0; j < BATCH_SIZE; j++) {
                print_debug("Writing %lu to offset %d\n", i*BATCH_SIZE + j, BLOCK_SIZE *j );
                snprintf(&payload[BLOCK_SIZE *(j)], 50, "Bye WORLD! I am done? %lu", i*BATCH_SIZE + j);
            }
            uint64_t wStart = getns();
            write_rmem_bulk(target_ip, payload, (struct in6_memaddr *)&r_addr[i * BATCH_SIZE], BATCH_SIZE);
            write_latency[i] = getns() - wStart;
        }
    } else {
        for (uint64_t i = 0; i < data->length; i++) {
            struct in6_memaddr remoteMemory = r_addr[i];
            print_debug("Creating payload");
            char payload[BLOCK_SIZE];
            snprintf(payload, 50, "Bye WORLD! I am done? %lu", i);
            uint64_t wStart = getns();
            write_rmem(target_ip, payload, &remoteMemory);
            write_latency2[i] = getns() - wStart;
        }
    }
    // READ TEST 2
    printf("Thread %d Starting second read test...\n", data->tid);
    for (uint64_t i = 0; i < data->length; i++) {
        struct in6_memaddr remoteMemory = r_addr[i];
        uint64_t rStart = getns();
        get_rmem(test, BLOCK_SIZE, target_ip, &remoteMemory);
        read_latency2[i] = getns() - rStart;
        char payload[BLOCK_SIZE];
        snprintf(payload, 50, "Bye WORLD! I am done? %lu", i);
        print_debug("Results of memory store: %.50s", test);
        if (strncmp(test, payload, 50) < 0) {
            perror(KRED"ERROR: WRONG RESULT"RESET);
            printf("Expected %s Got %s\n", payload, test );
            exit(1);
        }
    }
    // FREE TEST
    printf("Thread %d Freeing memory...\n", data->tid);
    uint64_t rStart = getns();
    free_rmem(target_ip, r_addr);
    free_latency[0] = getns() - rStart;

    printSendLat();
    int MAX_FNAME = 256;
    char fname[MAX_FNAME];
    snprintf(fname, MAX_FNAME, "alloc_t%d", data->tid);
    save_time(fname, alloc_latency, 1);
    memset(fname, 0, MAX_FNAME);
    snprintf(fname, MAX_FNAME, "read_t%d", data->tid);
    save_time(fname, read_latency, data->length);
    memset(fname, 0, MAX_FNAME);
    snprintf(fname, MAX_FNAME, "write_t%d", data->tid);
    save_time(fname, write_latency, data->length);
    snprintf(fname, MAX_FNAME, "free_t%d", data->tid);
    save_time(fname, free_latency, 1);
    memset(fname, 0, MAX_FNAME);

    close_sockets();
    return NULL;
}

void basic_op_threads(struct sockaddr_in6 *target_ip) {
    close_sockets();
    pthread_t thr[NUM_THREADS];
    int i;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];
    //struct config myConf = get_bb_config();

    pthread_attr_t attr;
    size_t  stacksize = 0;

    pthread_attr_init( &attr );
    pthread_attr_getstacksize( &attr, &stacksize );
    pthread_attr_setstacksize( &attr, 99800000 );
    pthread_attr_getstacksize( &attr, &stacksize );

    /* create threads */
    for (i = 0; i < NUM_THREADS; i++) {
        int rc;
        uint64_t split = NUM_ITERATIONS/NUM_THREADS;
        thr_data[i].tid =  i;
        thr_data[i].target_ip =  target_ip;
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
}


void basicOperations(struct sockaddr_in6 *target_ip) {

    // Initialize BlueBridge
    struct config myConf = get_bb_config();

    // Allocate the benchmarking arrays
    uint64_t alloc_latency[myConf.num_hosts];
    uint64_t *alloc_latency_single = calloc(NUM_ITERATIONS, sizeof(uint64_t));
    uint64_t *read_latency = calloc(NUM_ITERATIONS, sizeof(uint64_t));
    uint64_t *read_latency2 = calloc(NUM_ITERATIONS, sizeof(uint64_t));
    uint64_t *write_latency = calloc(NUM_ITERATIONS, sizeof(uint64_t));
    uint64_t *write_latency2 = calloc(NUM_ITERATIONS, sizeof(uint64_t));
    uint64_t free_latency[myConf.num_hosts];
    uint64_t *free_latency_single = calloc(NUM_ITERATIONS, sizeof(uint64_t));

    // Allocate the address space we will use
    struct in6_memaddr *r_addr = malloc(sizeof(struct in6_memaddr) * NUM_ITERATIONS);
    if(!r_addr)
        perror("Allocation too large");

    // ALLOC TEST BULK
    uint64_t split = NUM_ITERATIONS/myConf.num_hosts;
    int length;
    printf("Allocating...\n");
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t start = getns(); 
        uint64_t offset = split * i;
        if (i == myConf.num_hosts-1)
            length = NUM_ITERATIONS - offset;
        else 
            length = split;
        struct in6_addr *ipv6Pointer = gen_ip6_target(i);
        memcpy(&(target_ip->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        struct in6_memaddr *temp = allocate_rmem_bulk(target_ip, length);
        memcpy(&r_addr[offset],temp,length *sizeof(struct in6_memaddr) );
        free(temp);
        alloc_latency[i] = getns() - start; 
    }
    // ALLOC TEST SINGLE
    // for (int i = 0; i< NUM_ITERATIONS; i++) {
    //    // Generate a random IPv6 address out of a set of available hosts
    //     //memcpy(&(target_ip->sin6_addr), gen_rdm_IPv6Target(), sizeof(struct in6_addr));
    //     memcpy(&(target_ip->sin6_addr), gen_ip6_target(i % myConf.num_hosts ), sizeof(struct in6_addr));
    //     uint64_t start = getns(); 
    //     r_addr[i] = allocate_rmem(target_ip);
    //     alloc_latency_single[i] = getns() - start; 
    // }

    // WRITE TEST
    printf("Starting write test...\n");
    if (BATCHED_MODE) {
        for (uint64_t i = 0; i < NUM_ITERATIONS/BATCH_SIZE; i++) {
            char payload[BLOCK_SIZE * BATCH_SIZE];
            print_debug("Creating payload");
            for (int j = 0; j < BATCH_SIZE; j++) {
                print_debug("Writing %lu to offset %d\n", i*BATCH_SIZE + j, BLOCK_SIZE *j );
                snprintf(&payload[BLOCK_SIZE *(j)], 50, "HELLO WORLD! How are you? %lu", i*BATCH_SIZE + j);
            }
            uint64_t wStart = getns();
            write_rmem_bulk(target_ip, payload, (struct in6_memaddr *)&r_addr[i * BATCH_SIZE], BATCH_SIZE);
            write_latency[i] = getns() - wStart;
        }
    } else {
        for (uint64_t i = 0; i < NUM_ITERATIONS; i++) {
            struct in6_memaddr remoteMemory = r_addr[i];
            print_debug("Creating payload");
            char payload[BLOCK_SIZE];
            snprintf(payload, 50, "HELLO WORLD! How are you? %lu", i);
            uint64_t wStart = getns();
            write_rmem(target_ip, payload, &remoteMemory);
            write_latency[i] = getns() - wStart;
        }
    }
    char test[BLOCK_SIZE];
    // READ TEST
    printf("Starting read test...\n");
    for (uint64_t i = 0; i < NUM_ITERATIONS; i++) {
        struct in6_memaddr remoteMemory = r_addr[i];
        uint64_t rStart = getns();
        get_rmem(test, BLOCK_SIZE, target_ip, &remoteMemory);
        read_latency[i] = getns() - rStart;
        char payload[BLOCK_SIZE];
        snprintf(payload, 50, "HELLO WORLD! How are you? %lu", i);
        print_debug("Results of memory store: %.50s", test);
        if (strncmp(test, payload, 50) < 0) {
            perror(KRED"ERROR: WRONG RESULT"RESET);
            printf("Expected %s Got %s\n", payload, test );
            exit(1);
        }
    }
    // WRITE TEST 2
    printf("Starting second write test...\n");
    if (BATCHED_MODE) {
        for (uint64_t i = 0; i < NUM_ITERATIONS/BATCH_SIZE; i++) {
            char payload[BLOCK_SIZE * BATCH_SIZE];
            print_debug("Creating payload");
            for (int j = 0; j < BATCH_SIZE; j++) {
                print_debug("Writing %lu to offset %d\n", i*BATCH_SIZE + j, BLOCK_SIZE *j );
                snprintf(&payload[BLOCK_SIZE *(j)], 50, "Bye WORLD! I am done? %lu", i*BATCH_SIZE + j);
            }
            uint64_t wStart = getns();
            write_rmem_bulk(target_ip, payload, (struct in6_memaddr *)&r_addr[i * BATCH_SIZE], BATCH_SIZE);
            write_latency[i] = getns() - wStart;
        }
    } else {
        for (uint64_t i = 0; i < NUM_ITERATIONS; i++) {
            struct in6_memaddr remoteMemory = r_addr[i];
            print_debug("Creating payload");
            char payload[BLOCK_SIZE];
            snprintf(payload, 50, "Bye WORLD! I am done? %lu", i);
            uint64_t wStart = getns();
            write_rmem(target_ip, payload, &remoteMemory);
            write_latency2[i] = getns() - wStart;
        }
    }
    // READ TEST 2
    printf("Starting second read test...\n");
    for (uint64_t i = 0; i < NUM_ITERATIONS; i++) {
        struct in6_memaddr remoteMemory = r_addr[i];
        uint64_t rStart = getns();
        get_rmem(test, BLOCK_SIZE, target_ip, &remoteMemory);
        read_latency2[i] = getns() - rStart;
        char payload[BLOCK_SIZE];
        snprintf(payload, 50, "Bye WORLD! I am done? %lu", i);
        print_debug("Results of memory store: %.50s", test);
        if (strncmp(test, payload, 50) < 0) {
            perror(KRED"ERROR: WRONG RESULT"RESET);
            printf("Expected %s Got %s\n", payload, test );
            exit(1);
        }
    }

    //FREE TEST BULK
    printf("Freeing...\n");
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t fStart = getns();
        uint64_t offset = split * i;
        free_rmem(target_ip, &r_addr[offset]);
        free_latency[i] = getns() - fStart;
    }
    //FREE TEST Single
    // for (int i = 0; i < NUM_ITERATIONS; i++) {
    //     struct in6_memaddr remoteMemory = r_addr[i];
    //     uint64_t fStart = getns();
    //     free_rmem(target_ip, &remoteMemory);
    //     free_latency_single[i] = getns() - fStart;
    // }

    save_time("alloc_t0", alloc_latency, myConf.num_hosts);
    save_time("write_t0", write_latency, NUM_ITERATIONS);
    save_time("read_t0", read_latency, NUM_ITERATIONS);
    save_time("write2_t0", write_latency2, NUM_ITERATIONS);
    save_time("read2_t0", read_latency2, NUM_ITERATIONS);
    save_time("free_t0", free_latency, myConf.num_hosts);
    free(read_latency);
    free(write_latency);
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
    while ((c = getopt (argc, argv, "c:i:t:b")) != -1) { 
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
      case 'b': 
        BATCHED_MODE = 1; 
        break;
      case '?': 
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          printf("usage: -c config -t num_threads -i num_iterations>\n");
        return 1; 
      default: 
        abort (); 
      } 
    } 
    printf("Running test with %d threads and %lu iterations.\n", NUM_THREADS, NUM_ITERATIONS );
    struct sockaddr_in6 *temp = init_sockets(&myConf, 0);
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

