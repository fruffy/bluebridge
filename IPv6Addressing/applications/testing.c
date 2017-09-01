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

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define RESET "\033[0m"
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
    struct in6_addr AddrString;
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

void basicOperations(struct sockaddr_in6 * targetIP) {
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
    struct LinkedPointer * rootPointer = (struct LinkedPointer *) malloc( sizeof(struct LinkedPointer));
    struct LinkedPointer * nextPointer = rootPointer;
    //init the root element
    nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
    nextPointer->AddrString = allocateRemoteMem(targetIP);
    srand(time(NULL));
    for (i = 0; i < NUM_ITERATIONS; i++) {
        nextPointer = nextPointer->Pointer;
        nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
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
        struct in6_addr remoteMemory = nextPointer->AddrString;
        print_debug("Using Pointer: %p", (void *) getPointerFromIPv6(nextPointer->AddrString));
        print_debug("Creating payload");
        char *payload= malloc(50);
        memcpy(payload,"Hello World!", 10);

        uint64_t wStart = getns();
        writeRemoteMem(targetIP, payload, &remoteMemory);
        write_latency[i - 1] = getns() - wStart;
        uint64_t rStart = getns();
        char * test = getRemoteMem(targetIP, &remoteMemory);
        read_latency[i - 1] = getns() - rStart;

        print_debug("Results of memory store: %.50s", test);
        
        uint64_t fStart = getns();
        freeRemoteMem(targetIP, &remoteMemory);
        free_latency[i-1] = getns() - fStart;
        free(payload);
        free(test);
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
//TODO: Remove?
struct PointerMap {
    struct in6_addr Pointer;
};


/*
 * Interactive structure for debugging purposes
 */
void interactiveMode(struct sockaddr_in6 *targetIP) {
    long unsigned int len = 200;
    char input[len];
    char * localData;
    int count = 0;
    int i;
    struct in6_addr remotePointers[100];
    char * lazyZero = calloc(IPV6_SIZE, sizeof(char));
    char s[INET6_ADDRSTRLEN];
    
    // Initialize remotePointers array
    for (i = 0; i < 100; i++) {
        memset(remotePointers[i].s6_addr,0,IPV6_SIZE);
    }
    
    int active = 1;
    while (active) {
        srand(time(NULL));
        memset(input, 0, len);
        getLine("Please specify if you would like to (L)ist, (A)llocate, (F)ree, (W)rite, or (R)equest data.\nPress Q to quit the program.\n", input, sizeof(input));
        if (strcmp("A", input) == 0) {
            memset(input, 0, len);
            getLine("Specify a number of address or press enter for one.\n", input, sizeof(input));

            if (strcmp("", input) == 0) {
                printf("Calling allocateRemoteMem\n");              
                struct in6_addr remoteMemory = allocateRemoteMem(targetIP);
                inet_ntop(targetIP->sin6_family,(struct sockaddr *) &remoteMemory.s6_addr, s, sizeof s);
                printf("Got this pointer from call%s\n", s);
                memcpy(&remotePointers[count++], &remoteMemory, sizeof(remoteMemory));
            } else {
                int num = atoi(input);
                printf("Received %d as input\n", num);
                int j; 
                for (j = 0; j < num; j++) {
                    printf("Calling allocateRemoteMem\n");
                    struct in6_addr remoteMemory = allocateRemoteMem(targetIP);                    
                    inet_ntop(targetIP->sin6_family,(struct sockaddr *) &remoteMemory.s6_addr, s, sizeof s);
                    printf("Got this pointer from call%s\n", s);
                    printf("Creating pointer to remote memory address\n");
                    memcpy(&remotePointers[count++], &remoteMemory, sizeof(remoteMemory));
                }
            }
        } else if (strcmp("L", input) == 0){
            printf("Remote Address Pointer\n");
            for (i = 0; i < 100; i++){
                if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
                    inet_ntop(targetIP->sin6_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
                    printf("%s\n", s);
                }
            }
        } else if (strcmp("R", input) == 0) {
            memset(input, 0, len);
            getLine("Enter C to read a custom memory address. A to read all pointers.\n", input, sizeof(input));

            if (strcmp("A", input) == 0) {
                for (i = 0; i < 100; i++) {
                    if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
                        inet_ntop(targetIP->sin6_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
                        printf("Using pointer %s to read\n", s);
                        localData = getRemoteMem(targetIP, &remotePointers[i]);
                        printf("Retrieved Data (first 80 bytes): %.*s\n", 80, localData);
                    }
                }
            } else if (strcmp("C", input) == 0) {
                memset(input, 0, len);
                getLine("Please specify the target pointer:\n", input, sizeof(input));
                struct in6_addr pointer;
                inet_pton(AF_INET6, input, &pointer);
                inet_ntop(targetIP->sin6_family,(struct sockaddr *) &pointer.s6_addr, s, sizeof s);
                printf("Reading from this pointer %s\n", s);
                localData = getRemoteMem(targetIP, &pointer);
                printf(KCYN "Retrieved Data (first 80 bytes):\n");
                printf("****************************************\n");
                printf("\t%.*s\t\n",80, localData);
                printf("****************************************\n");
                printf(RESET);
            }
        } else if (strcmp("W", input) == 0) {
            memset(input, 0, len);
            getLine("Enter C to write to a custom memory address. A to write to all pointers.\n", input, sizeof(input));

            if (strcmp("A", input) == 0) {
                for (i = 0; i < 100; i++) {
                    if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
                        inet_ntop(targetIP->sin6_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);                  
                        printf("Writing to pointer %s\n", s);
                        memset(input, 0, len);
                        getLine("Please enter your data:\n", input, sizeof(input));
                        if (strcmp("", input) == 0) {
                            printf("Writing random bytes\n");
                            unsigned char * payload = gen_rdm_bytestream(BLOCK_SIZE);
                            writeRemoteMem(targetIP, (char*) payload, &remotePointers[i]);
                        } else {
                            printf(KMAG "Writing:\n");
                            printf("****************************************\n");
                            printf("\t%.*s\t\n",80, input);
                            printf("****************************************\n");
                            printf(RESET);
                            writeRemoteMem(targetIP, input, &remotePointers[i]);   
                        }
                    }
                }
            } else if (strcmp("C", input) == 0) {
                memset(input, 0, len);
                getLine("Please specify the target pointer:\n", input, sizeof(input));
                struct in6_addr pointer;
                inet_pton(AF_INET6, input, &pointer);
                inet_ntop(targetIP->sin6_family,(struct sockaddr *) pointer.s6_addr, s, sizeof s);
                printf("Writing to pointer %s\n", s);
                memset(input, 0, len);
                getLine("Please enter your data:\n", input, sizeof(input));
                if (strcmp("", input) == 0) {
                    printf("Writing random bytes\n");
                    unsigned char * payload = gen_rdm_bytestream(BLOCK_SIZE);
                    writeRemoteMem(targetIP, (char*) payload, &remotePointers[i]);
                } else {
                    printf("Writing: %s\n", input);
                    writeRemoteMem(targetIP, input, &remotePointers[i]);   
                }
            }
        } else if (strcmp("M", input) == 0) {
            memset(input, 0, len);
            getLine("Enter C to free a custom memory address. A to migrate all pointers.\n", input, sizeof(input));
            
            if (strcmp("A", input) == 0) {
                for (i = 0; i < 100; i++) {
                    if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
                        inet_ntop(targetIP->sin6_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);                  
                        memset(input, 0, len);
                        printf("Migrating pointer %s\n", s);
                        getLine("Please enter your migration machine:\n", input, sizeof(input));
                        if (atoi(input) <= NUM_HOSTS) {
                            printf("Migrating\n");
                            migrateRemoteMem(targetIP, &remotePointers[i], atoi(input));
                        } else {
                            printf("FAILED\n"); 
                        }
                    }
                }
            } else if (strcmp("C", input) == 0) {
                memset(input, 0, len);
                getLine("Please specify the target pointer:\n", input, sizeof(input));
                struct in6_addr pointer;
                inet_pton(AF_INET6, input, &pointer);
                inet_ntop(targetIP->sin6_family,(struct sockaddr *) pointer.s6_addr, s, sizeof s);
                printf("Migrating pointer %s\n", s);
                memset(input, 0, len);
                getLine("Please enter your migration machine:\n", input, sizeof(input));
                if (atoi(input) <= NUM_HOSTS) {
                    printf("Migrating\n");
                    migrateRemoteMem(targetIP, &pointer, atoi(input));
                } else {
                    printf("FAILED\n"); 
                }
            }
        } else if (strcmp("F", input) == 0) {
            memset(input, 0, len);
            getLine("Enter C to free a custom memory address. A to free all pointers.\n", input, sizeof(input));
            
            if (strcmp("A", input) == 0) {
                for (i = 0; i < 100; i++) {
                    if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
                        inet_ntop(targetIP->sin6_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
                        printf("Freeing pointer %s\n", s);
                        freeRemoteMem(targetIP, &remotePointers[i]);
                        memset(remotePointers[i].s6_addr,0, IPV6_SIZE);
                    }
                }   
            } else if (strcmp("C", input) == 0) {
                memset(input, 0, len);
                getLine("Please specify the target pointer:\n", input, sizeof(input));
                struct in6_addr pointer;
                inet_pton(AF_INET6, input, &pointer);
                inet_ntop(targetIP->sin6_family,(struct sockaddr *) &pointer.s6_addr, s, sizeof s);
                printf("Freeing pointer%s\n", s);               freeRemoteMem(targetIP, &pointer);
                for (i = 0; i < 100; i++) {
                    if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
                        memset(remotePointers[i].s6_addr,0, IPV6_SIZE);
                    }
                }
            }
        } else if (strcmp("Q", input) == 0) {
            active = 0;
            printf("Ende Gelaende\n");
        } else {
            printf("Try again.\n");
        }
    }
    free(lazyZero);
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
    struct config myConf = configure_bluebridge("tmp/config/distMem.cnf", 0);

    struct sockaddr_in6 *temp = init_rcv_socket(&myConf);
//    struct sockaddr_in6 *temp = init_rcv_socket_old(argv[2]);
    temp->sin6_port = htons(strtol(myConf.server_port, (char **)NULL, 10));
    init_send_socket(&myConf);
    struct timeval st, et;
    gettimeofday(&st,NULL);
    if(isAutoMode) {
        basicOperations(temp);
    } else {
        interactiveMode(temp);
    }
    gettimeofday(&et,NULL);
    int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec);
    printf("Total Time: %d micro seconds\n",elapsed);
    printf(KRED "Finished\n");
    printf(RESET);
    printSendLat();
    free(temp);
    close_sockets();
    return 0;
}

