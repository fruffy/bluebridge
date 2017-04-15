#include <linux/userfaultfd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <inttypes.h>

#include "./lib/538_utils.h"
#include "./lib/debug.h"

#define NUM_PAGES 1000

static volatile int stop;
void *region;

struct params {
    int uffd;
    long page_size;
};

static uint64_t addr_map_int[NUM_PAGES];
static struct in6_addr* addr_map_in6[NUM_PAGES];

static inline uint64_t getns(void)
{
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(ret == 0);
    return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

static long get_page_size(void)
{
    long ret = sysconf(_SC_PAGESIZE);
    if (ret == -1) {
        perror("sysconf/pagesize");
        exit(1);
    }
    assert(ret > 0);
    return ret;
}

struct in6_addr getRemoteAddr(int sockfd, struct addrinfo *p, uint64_t pointer) {
    char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
    char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));

    int size = 0;

    memcpy(sendBuffer, GET_ADDR_CMD, sizeof(GET_ADDR_CMD));
    size += sizeof(GET_ADDR_CMD) - 1; // Should be 4
    memcpy(sendBuffer+size, &pointer, POINTER_SIZE);
    size += POINTER_SIZE; // Should be 8, total 12
    // Lines are for ndpproxy DO NOT REMOVE
    struct in6_addr * ipv6Pointer = gen_fixed_IPv6Target(1);
    memcpy(&(((struct sockaddr_in6*) p->ai_addr)->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
    p->ai_addrlen = sizeof(*ipv6Pointer);
    
    // printf("Retrieving address for pointer: %" PRIx64 "\n", pointer);

    // Send message
    sendUDP(sockfd, sendBuffer,BLOCK_SIZE, p);
    // Receive response
    receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);

    struct in6_addr retVal;

    if (memcmp(receiveBuffer,"ACK",3) == 0) {
        print_debug("Response was ACK");
        // If the message is ACK --> successful
        struct in6_addr * remotePointer = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
        // printf("Received IPv6 data: ");
        // printNBytes(receiveBuffer+4,IPV6_SIZE);
        // Copy the returned pointer
        memcpy(remotePointer, receiveBuffer+4, IPV6_SIZE);
        // Insert information about the source host (black magic)
        //00 00 00 00 01 01 00 00 00 00 00 00 00 00 00 00
        //            ^  ^ these two bytes are stored (subnet and host ID)
        memcpy(remotePointer->s6_addr+4, get_in_addr(p->ai_addr)+4, 2);
        print_debug("Got: %p from server", (void *)remotePointer);

        retVal = *remotePointer;
    } else {
        fprintf(stderr, "PANIC\n");
    }

    free(sendBuffer);

    // Parse receive buffer
    return retVal;
}

char * getMemory(int sockfd, struct addrinfo * p, struct in6_addr * toPointer) {
    char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
    char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));

    int size = 0;

    // Prep message
    memcpy(sendBuffer, GET_CMD, sizeof(GET_CMD));
    size += sizeof(GET_CMD) - 1; // Should be 4
    memcpy(sendBuffer+size,toPointer->s6_addr,IPV6_SIZE);
    size += IPV6_SIZE; // Should be 8, total 12

    // Send message
    sendUDPIPv6(sockfd, sendBuffer,BLOCK_SIZE, p,*toPointer);
    // Receive response
    receiveUDP(sockfd, receiveBuffer,BLOCK_SIZE, p);


    free(sendBuffer);

    return receiveBuffer;
}

void print_times(uint64_t* first_rtt_start, uint64_t* first_rtt_end, uint64_t* second_rtt_start, uint64_t* second_rtt_end, uint64_t* handler_start, uint64_t* handler_end) {
    FILE *f1 = fopen("first_rtt_times.csv", "w");
    if (f1 == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *f2 = fopen("second_rtt_times.csv", "w");
    if (f2 == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *f3 = fopen("handler_times.csv", "w");
    if (f3 == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    fprintf(f1, "start (ns),end (ns)\n");
    fprintf(f2, "start (ns),end (ns)\n");
    fprintf(f3, "start (ns),end (ns)\n");

    int i;
    for (i = 0; i < NUM_PAGES; i++) {
        fprintf(f1, "%llu,%llu\n", (unsigned long long) first_rtt_start[i], (unsigned long long) first_rtt_end[i]);
        fprintf(f2, "%llu,%llu\n", (unsigned long long) second_rtt_start[i], (unsigned long long) second_rtt_end[i]);
        fprintf(f3, "%llu,%llu\n", (unsigned long long) handler_start[i], (unsigned long long) handler_end[i]);
    }

    fclose(f1);
    fclose(f2);
    fclose(f3);
}

static void *handler(void *arg)
{
    struct params *param = arg;
    long page_size = param->page_size;
///*
    // Setup network connections to server and directory server.
    int sockfd_server;
    struct addrinfo hints, *servinfo_server, *p_server;
    int rv;
    
    //Socket operator variables
    const int on=1, off=0;

    //Routing configuration
    // This is a temporary solution to enable the forwarding of unknown ipv6 subnets
    //printf("%d\n",system("sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo"));
    //ip -6 route add local ::3131:0:0:0:0/64  dev h1-eth0
    //ip -6 route add local ::3131:0:0:0:0/64  dev h2-eth0
    // Tells the getaddrinfo to only return sockets
    // which fit these params.
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "5000", &hints, &servinfo_server)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and connect to the first we can
    for (p_server = servinfo_server; p_server != NULL; p_server = p_server->ai_next) {
        if ((sockfd_server = socket(p_server->ai_family, p_server->ai_socktype, p_server->ai_protocol))
                == -1) {
            perror("client: socket");
            continue;
        }
        setsockopt(sockfd_server, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
        setsockopt(sockfd_server, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
        setsockopt(sockfd_server, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
        break;
    }
//*/

    uint64_t *first_rtt_start = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(first_rtt_start);
    memset(first_rtt_start, 0, sizeof(uint64_t) * NUM_PAGES);

    uint64_t *first_rtt_end = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(first_rtt_end);
    memset(first_rtt_end, 0, sizeof(uint64_t) * NUM_PAGES);

    uint64_t *second_rtt_start = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(second_rtt_start);
    memset(second_rtt_start, 0, sizeof(uint64_t) * NUM_PAGES);

    uint64_t *second_rtt_end = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(second_rtt_end);
    memset(second_rtt_end, 0, sizeof(uint64_t) * NUM_PAGES);

    uint64_t *handler_start = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(handler_start);
    memset(handler_start, 0, sizeof(uint64_t) * NUM_PAGES);

    uint64_t *handler_end = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(handler_end);
    memset(handler_end, 0, sizeof(uint64_t) * NUM_PAGES);

    int i = 0;

    for (;;) {
        handler_start[i] = getns();
        struct uffd_msg msg;
        char buf[page_size];

        struct pollfd pollfd[1];
        pollfd[0].fd = param->uffd;
        pollfd[0].events = POLLIN;

        // wait for a userfaultfd event to occur
        int pollres = poll(pollfd, 1, 2000);

        if (stop)
            goto EXIT;

        switch (pollres) {
        case -1:
            perror("poll/userfaultfd");
            continue;
        case 0:
            continue;
        case 1:
            break;
        default:
            fprintf(stderr, "unexpected poll result\n");
            exit(1);
        }

        if (pollfd[0].revents & POLLERR) {
            fprintf(stderr, "pollerr\n");
            exit(1);
        }

        if (!pollfd[0].revents & POLLIN) {
            continue;
        }

        int readres = read(param->uffd, &msg, sizeof(msg));
        if (readres == -1) {
            if (errno == EAGAIN)
                continue;
            perror("read/userfaultfd");
            exit(1);
        }

        if (readres != sizeof(msg)) {
            fprintf(stderr, "invalid msg size\n");
            exit(1);
        }

        // handle the page fault by copying a page worth of bytes
        if (msg.event & UFFD_EVENT_PAGEFAULT) {
            // TODO: Add dictionary or array of remote addresses. 
            //       Check to see if the pagefault is remote or local
            //       If remote, do the get memory call. 
            //       If local, do this...
            long long addr = msg.arg.pagefault.address;

            // fprintf(stdout, "Address: %"PRIx64"\n", (uint64_t) msg.arg.pagefault.address);
            uint64_t start_addr = (uint64_t) region;
            int index = (int) (addr - start_addr)/get_page_size();

            // fprintf(stdout, "Address: %"PRIx64", index: %d\n", (uint64_t) msg.arg.pagefault.address, index);

            if (addr_map_int[index] > 0 || addr_map_in6[index] != NULL) {
                // Do remote things
                // 1. Get machine from the remote directory service server
                //      a. Prepare packet, send to DS, receive IPv6 address back

                // 2. Get stuff from the remote server
                //      a. getMemory() from client.c
                struct in6_addr remoteMachine;

                if (addr_map_in6[index] == NULL) {
                    printf("Not IPv6\n");
                    first_rtt_start[i] = getns();
                    remoteMachine = getRemoteAddr(sockfd_server, p_server, addr_map_int[index]);
                    first_rtt_end[i] = getns();

                    char s[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, &remoteMachine, s, sizeof(s));
                    fprintf(stdout, "Remote Machine is... %s\n", s);
                } else {
                    printf("IPv6\n");
                    remoteMachine = *addr_map_in6[index];

                    char s[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, &remoteMachine, s, sizeof(s));
                    fprintf(stdout, "Remote Machine is... %s\n", s);
                }
                second_rtt_start[i] = getns();
                char* val = getMemory(sockfd_server, p_server, &remoteMachine);
                second_rtt_end[i] = getns();
                
                struct uffdio_copy copy;
                copy.src = (long long)buf;
                copy.dst = (long long)addr;
                copy.len = page_size;
                copy.mode = 0;
                if (ioctl(param->uffd, UFFDIO_COPY, &copy) == -1) {
                    perror("ioctl/copy");
                    exit(1);
                }

                handler_end[i] = getns();
                i++;
                // printf("Results of memory store: %.50s\n", val);
            } else {
                // handler_start[i] = getns();

                FILE *f;
                // char cmdname[8192];
                // snprintf(cmdname, sizeof(cmdname), "zcat tmp%d.gz", 1);
                f = fopen("temp.txt", "r");
                if (f == NULL) {
                    perror("open temp.txt");
                    exit(1);
                }
                // char buf[page_size];
                if (fread(buf, page_size, 1, f) != 0) {
                    perror("fread");
                    exit(1);
                }
                if (fclose(f)) {
                    perror("fclose");
                    exit(1);
                }

                struct uffdio_copy copy;
                copy.src = (long long)buf;
                copy.dst = (long long)addr;
                copy.len = page_size;
                copy.mode = 0;
                if (ioctl(param->uffd, UFFDIO_COPY, &copy) == -1) {
                    perror("ioctl/copy");
                    exit(1);
                }

                handler_end[i] = getns();
                i++;
            }
        }

    }

EXIT:

    print_times(first_rtt_start, first_rtt_end, second_rtt_start, second_rtt_end, handler_start, handler_end);

    freeaddrinfo(servinfo_server);
    close(sockfd_server);
    free(first_rtt_start);
    free(second_rtt_start);
    free(first_rtt_end);
    free(second_rtt_end);
    free(handler_start);
    free(handler_end);

    return NULL;
}

struct in6_addr allocateMem(int sockfd, struct addrinfo * p) {
    char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
    char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));

    // Lines are for ndpproxy DO NOT REMOVE
    struct in6_addr * ipv6Pointer = gen_rdm_IPv6Target();
    memcpy(&(((struct sockaddr_in6*) p->ai_addr)->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
    p->ai_addrlen = sizeof(*ipv6Pointer);
    
    memcpy(sendBuffer, ALLOC_CMD, sizeof(ALLOC_CMD));

    sendUDP(sockfd, sendBuffer,BLOCK_SIZE, p);
    // Wait to receive a message from the server
    int numbytes = receiveUDP(sockfd, receiveBuffer, BLOCK_SIZE, p);
    print_debug("Received %d bytes", numbytes);
    // Parse the response

    struct in6_addr retVal;

    if (memcmp(receiveBuffer,"ACK",3) == 0) {
        print_debug("Response was ACK");
        // If the message is ACK --> successful
        struct in6_addr * remotePointer = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
        // Copy the returned pointer
        memcpy(remotePointer, receiveBuffer+4, IPV6_SIZE);
        // Insert information about the source host (black magic)
        //00 00 00 00 01 02 00 00 00 00 00 00 00 00 00 00
        //            ^  ^ these two bytes are stored (subnet and host ID)
        memcpy(remotePointer->s6_addr+4, get_in_addr(p->ai_addr)+4, 2);
        print_debug("Got: %p from server", (void *)remotePointer);
        retVal = *remotePointer;
    } else {
        print_debug("Response was not successful");
        // Not successful so we send another message?
        memcpy(sendBuffer, "What's up?", sizeof("What's up?"));
        sendUDP(sockfd, sendBuffer,BLOCK_SIZE, p);

        print_debug("Keeping return value as 0");
    }

    free(sendBuffer);
    free(receiveBuffer);

    return retVal;
}

int main(int argc, char **argv)
{
    int uffd;
    long page_size;
    pthread_t uffd_thread;
    int measure_write = 1; // 1 = write, 0 = read
    int remote = 1; // 0 = local, 1 = remote
    int directService = 0; // 1 = directory service, 0 = no directory service

    page_size = get_page_size();

    fprintf(stdout, "Page size: %ld\nNumber of pages: %d\n", page_size, NUM_PAGES);

    // open the userfault fd
    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1) {
        perror("syscall/userfaultfd");
        exit(1);
    }

    // enable for api version and check features
    struct uffdio_api uffdio_api;
    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
        perror("ioctl/uffdio_api");
        exit(1);
    }

    if (uffdio_api.api != UFFD_API) {
        fprintf(stderr, "unsupported userfaultfd api\n");
        exit(1);
    }

    // allocate a memory region to be managed by userfaultfd
    region = mmap(NULL, page_size * NUM_PAGES, PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // register the pages in the region for missing callbacks
    // TODO: need to modify this such that it registers half the pages
    //       to the server (i.e. creates remote pointers for half the pages)
    struct uffdio_register uffdio_register;
    uffdio_register.range.start = (uint64_t)region;
    uffdio_register.range.len = page_size * NUM_PAGES;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        perror("ioctl/uffdio_register");
        exit(1);
    }

    // Setup network connections to server and directory server.
    int sockfd_server;
    struct addrinfo hints, *servinfo_server, *p_server;
    int rv;
    
    //Socket operator variables
    const int on=1, off=0;

    //Routing configuration
    // This is a temporary solution to enable the forwarding of unknown ipv6 subnets
    //printf("%d\n",system("sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo"));
    //ip -6 route add local ::3131:0:0:0:0/64  dev h1-eth0
    //ip -6 route add local ::3131:0:0:0:0/64  dev h2-eth0
    // Tells the getaddrinfo to only return sockets
    // which fit these params.
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "5000", &hints, &servinfo_server)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p_server = servinfo_server; p_server != NULL; p_server = p_server->ai_next) {
        if ((sockfd_server = socket(p_server->ai_family, p_server->ai_socktype, p_server->ai_protocol))
                == -1) {
            perror("client: socket");
            continue;
        }
        setsockopt(sockfd_server, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
        setsockopt(sockfd_server, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
        setsockopt(sockfd_server, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
        break;
    }

    // Make every x regions remote
    long i;
    char *cur = region;
    for (i = 0; i < NUM_PAGES; i++) {
        if (remote) {
            // Allocate remote memory
            if (directService) {
                addr_map_int[i] = getPointerFromIPv6(allocateMem(sockfd_server, p_server));
            } else {
                struct in6_addr * remotePointer = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
                struct in6_addr temp = allocateMem(sockfd_server, p_server);

                memcpy(remotePointer, &temp, IPV6_SIZE);
                addr_map_in6[i] = remotePointer;
            }
        } else {
            if (directService) {
                addr_map_int[i] = 0;
            } else {
                addr_map_in6[i] = NULL;
            }
        }
    }

    if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) !=
            UFFD_API_RANGE_IOCTLS) {
        fprintf(stderr, "unexpected userfaultfd ioctl set\n");
        exit(1);
    }


    // start the thread that will handle userfaultfd events
    stop = 0;

    struct params p;
    p.uffd = uffd;
    p.page_size = page_size;

    pthread_create(&uffd_thread, NULL, handler, &p);

    sleep(1);

    // track the total latencies for each page
    uint64_t *total_start = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(total_start);
    memset(total_start, 0, sizeof(uint64_t) * NUM_PAGES);

    uint64_t *total_end = malloc(sizeof(uint64_t) * NUM_PAGES);
    assert(total_end);
    memset(total_end, 0, sizeof(uint64_t) * NUM_PAGES);
    // touch each page in the region
    cur = region;

    if (measure_write) {
        char val = 0x21; // Start at 21, stop at 0x7E

        // Write value --> faults b/c page not in memory
        for (i = 0; i < NUM_PAGES; i++) {
            total_start[i] = getns();
            // int v = *((int*)cur);
            *cur = val; // Writes the letter C, TODO: figure out how to make it do a full string
            total_end[i] = getns();
            cur += page_size;
            val++;
            if (val > 0x7E) {
                val = 0x21;
            }
        }
    } else {
        // Read value --> doesn't fault b/c page in memory from write
        cur = region;
        int value = 0;
        for (i = 0; i < NUM_PAGES; i++) {
            total_start[i] = getns();
            char v = *cur; // Get the current value as a char
            total_end[i] = getns();
            value += (int)v;
            cur += page_size;
        }
    }


    stop = 1;
    pthread_join(uffd_thread, NULL);

    if (ioctl(uffd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
        fprintf(stderr, "ioctl unregister failure\n");
        return 1;
    }

    // long double average, /*variance, std_deviation, */sum = 0/*, sum1 = 0*/;
    // uint64_t min = UINT64_MAX, max = 0;

    // for (i = 0; i < NUM_PAGES; i++) {
    //     sum += (long double) total_latencies[i];
    //     if (total_latencies[i] > max) {
    //         max = total_latencies[i];
    //     }

    //     if (total_latencies[i] < min) {
    //         min = total_latencies[i];
    //     }
    // }

    // average = sum / (long double) NUM_PAGES;

    /*  Compute  variance  and standard deviation  */

    // for (i = 0; i < NUM_PAGES; i++) {
    //     sum1 += ((long double) total_latencies[i] - average)*((long double) total_latencies[i] - average);
    // }

    // variance = sum1 / (long double)NUM_PAGES;

    // std_deviation = sqrt(variance);

    // Prints the total_latencies per page. Want just an average, median, min, max. 
    // for (i = 0; i < NUM_PAGES; i++) {
    //     fprintf(stdout, "%llu\n", (unsigned long long)total_latencies[i]);
    // }
    // fprintf(stdout, "Start: %"PRIx64"\n", (uint64_t) region);
    // fprintf(stdout, "Mean: %Lf\nMinimum: %llu\nMaximum: %llu\n", average, (unsigned long long) min, (unsigned long long) max);

    FILE *f = fopen("total_times.csv", "w");
    if (f == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    fprintf(f, "start (ns),end (ns)\n");

    for (i = 0; i < NUM_PAGES; i++) {
        fprintf(f, "%llu,%llu\n", (unsigned long long) total_start[i], (unsigned long long) total_end[i]);
    }

    free(total_start);
    free(total_end);
    munmap(region, page_size * NUM_PAGES);

    return 0;
}
