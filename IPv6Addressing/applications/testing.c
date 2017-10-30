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

const int NUM_ITERATIONS = 10000;

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
//  8. Integrate Mihir's asynchronous code and use raw linux threading:
//      http://nullprogram.com/blog/2015/05/15/
///////////////////////////////////////////////////////////////////////////////

//To add the current correct route
//sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo
//ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dest=0:0:01ff:0:ffff:ffff:0:0,actions=output:2
struct LinkedPointer {
    struct in6_memaddr AddrString;
    struct LinkedPointer * Pointer;
};


void print_times( uint64_t* alloc_latency, uint64_t* read_latency, uint64_t* write_latency, uint64_t* free_latency){
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

    for (i = 0; i < NUM_ITERATIONS; i++) {
        alloc_total += (unsigned long long) alloc_latency[i];
        read_total += (unsigned long long) read_latency[i];
        write_total += (unsigned long long) write_latency[i];
        free_total += (unsigned long long) free_latency[i];
    }
    printf("Average allocate latency: "KRED"%lu us\n"RESET, alloc_total/ (NUM_ITERATIONS*1000));
    printf("Average read latency: "KRED"%lu us\n"RESET, read_total/ (NUM_ITERATIONS*1000));
    printf("Average write latency: "KRED"%lu us\n"RESET, write_total/ (NUM_ITERATIONS*1000));
    printf("Average free latency: "KRED"%lu us\n"RESET, free_total/ (NUM_ITERATIONS*1000));
}

void basicOperations(struct sockaddr_in6 *targetIP) {
    uint64_t *alloc_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    assert(alloc_latency);
    memset(alloc_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);

    uint64_t *read_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    assert(read_latency);
    memset(read_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);

    uint64_t *write_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);

    assert(write_latency);
    memset(write_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);

    uint64_t *free_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    assert(free_latency);
    memset(free_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);
    int i;
    // Initialize remotePointers array
    struct LinkedPointer *rootPointer = (struct LinkedPointer *) malloc( sizeof(struct LinkedPointer));
    struct LinkedPointer *nextPointer = rootPointer;
    //init the root element
    nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
    // Generate a random IPv6 address out of a set of available hosts
    struct in6_addr *ipv6Pointer = gen_rdm_IPv6Target();
    memcpy(&(targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
    nextPointer->AddrString = allocateRemoteMem(targetIP);
    srand(time(NULL));
    for (i = 0; i < NUM_ITERATIONS; i++) {
        nextPointer = nextPointer->Pointer;
        nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
        // Generate a random IPv6 address out of a set of available hosts
        struct in6_addr *ipv6Pointer = gen_rdm_IPv6Target();
        memcpy(&(targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        uint64_t start = getns(); 
        nextPointer->AddrString = allocateRemoteMem(targetIP);
        alloc_latency[i] = getns() - start; 
    }
    // don't point to garbage
    // temp fix
    nextPointer->Pointer = NULL;
    i = 1;
    srand(time(NULL));
    nextPointer = rootPointer;
    while(nextPointer != NULL)  {

        print_debug("Iteration %d", i);
        struct in6_memaddr remoteMemory = nextPointer->AddrString;
        //print_debug("Using Pointer: %p", (void *) getPointerFromIPv6(nextPointer->AddrString));
        print_debug("Creating payload");
        char *payload= malloc(50);
        memcpy(payload,"Hello World!", 10);

        uint64_t wStart = getns();
        writeRemoteMem(targetIP, payload, &remoteMemory);
        write_latency[i - 1] = getns() - wStart;
        uint64_t rStart = getns();
        char *test = getRemoteMem(targetIP, &remoteMemory);
        read_latency[i - 1] = getns() - rStart;

        print_debug("Results of memory store: %.50s", test);
        if (strncmp(test,"Hello World!", 10) < 0) {
            print_debug(KRED"ERROR: WRONG RESULT"RESET);
            exit(1);
        }
        uint64_t fStart = getns();
        freeRemoteMem(targetIP, &remoteMemory);
        free_latency[i-1] = getns() - fStart;
        free(payload);
        nextPointer = nextPointer->Pointer;
        i++;
    }
    nextPointer = rootPointer;
    while (nextPointer != NULL) {
        rootPointer = nextPointer; 
        nextPointer = nextPointer->Pointer;
        free (rootPointer);
    }
    print_times(alloc_latency, read_latency, write_latency, free_latency);

    free(alloc_latency);
    free(write_latency);
    free(read_latency);
    free(free_latency);
}

/* create thread argument struct for thr_func() */
#include <pthread.h>
#define NUM_THREADS 10

typedef struct _thread_data_t {
  int tid;
  struct in6_memaddr *r_addr;
  struct sockaddr_in6 *targetIP;
  int length;
} thread_data_t;
struct config myConf;
void *testing_loop(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    //struct sockaddr_in6 *temp = calloc(1,sizeof(struct sockaddr_in6));//init_rcv_socket(&myConf);
    struct sockaddr_in6 *temp = init_rcv_socket(&myConf);
    temp->sin6_port = htons(strtol(myConf.server_port, (char **)NULL, 10));
    init_send_socket(&myConf);
    set_thread_id_sd(data->tid);
    set_thread_id_rx(data->tid);

    for (int i = 0; i < data->length; i++) {
        print_debug("Writing Memory Iteration %d", i);
        struct in6_memaddr *remoteMemory = data->r_addr + i;
        //print_debug("Using Pointer: %p", (void *) getPointerFromIPv6(nextPointer->AddrString));
        print_debug("Creating payload");
        char *payload= malloc(50);
        snprintf(payload, 50, "HELLO WORLD! MY ID is: %d", data->tid);
        writeRemoteMem(temp, payload, remoteMemory);
        free(payload);
    }
    for (int i = 0; i < data->length; i++) {
        struct in6_memaddr *remoteMemory = data->r_addr + i;
        char *test = getRemoteMem(temp, remoteMemory);
        print_debug("Thread: %d, Results of memory store: %s\n",  data->tid, test);
        char *payload= malloc(50);
        snprintf(payload, 50, "HELLO WORLD! MY ID is: %d", data->tid);
        if (strncmp(test,payload, 50) < 0) {
            print_debug(KRED"ERROR: WRONG RESULT"RESET);
            exit(1);
        }
        free(payload);
    }
    for (int i = 0; i < data->length; i++) {
        struct in6_memaddr *remoteMemory = data->r_addr + i;
        freeRemoteMem(temp, remoteMemory);
    } 
    return NULL;
}

void basic_op_threads(struct sockaddr_in6 *targetIP) {
    
    pthread_t thr[NUM_THREADS];
    int i;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];

    // Generate a random IPv6 address out of a set of available hosts
    memcpy(&(targetIP->sin6_addr), gen_rdm_IPv6Target(), sizeof(struct in6_addr));
    struct in6_memaddr r_addr[NUM_ITERATIONS];
    srand(time(NULL));
    for (i = 0; i< NUM_ITERATIONS; i++) {
        r_addr[i] = allocateRemoteMem(targetIP);
    }
    close_sockets();
   /* create threads */
    for (i = 0; i < NUM_THREADS; i++) {
        int rc;
        int split = NUM_ITERATIONS/NUM_THREADS;
        thr_data[i].tid =  i;
        thr_data[i].targetIP =  targetIP;
        thr_data[i].r_addr =  &r_addr[split *i];
        thr_data[i].length = split;
        printf("Launching thread %d\n", i );
        if ((rc = pthread_create(&thr[i], NULL, testing_loop, &thr_data[i]))) {
          fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        }
    }
    /* block until all threads complete */
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(thr[i], NULL);
    }
}

/*
 * Main workhorse method. Parses arguments, setups connections
 * Allows user to issue commands on the command line.
 */
int main(int argc, char *argv[]) {

    
    //specify interactive or automatic client mode
    int isAutoMode = 1;
    //Socket operator variables
    
    int c;
    opterr = 0;
    while ((c = getopt (argc, argv, ":i")) != -1) {
    switch (c)
      {
      case 'i':
        isAutoMode = 0;
        break;
      case '?':
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        return 1;
      default:
        abort ();
      }
    }
    myConf = configure_bluebridge(argv[1], 0);

    struct sockaddr_in6 *temp = init_rcv_socket(&myConf);
//    struct sockaddr_in6 *temp = init_rcv_socket_old(argv[2]);
    temp->sin6_port = htons(strtol(myConf.server_port, (char **)NULL, 10));

    init_send_socket(&myConf);
    set_host_list(myConf.hosts, myConf.num_hosts);

    struct timeval st, et;
    gettimeofday(&st,NULL);
    //basicOperations(temp);
    basic_op_threads(temp);
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

