#define _GNU_SOURCE

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
#include <ctype.h>

#include "./lib/client_lib.h"
#include "./lib/udpcooked.h"

#define NUM_PAGES 78 // Specific to hound_of_baskerville_small file (size / 4096)

/////////////////////////////////// TO DOs ////////////////////////////////////
//  2. Add usage
//  3. Clean up all memory leaks (use GOTOs instead of just returning or exit())
//  4. Plan integration of this code and testing.c
//  5. Make method comments
///////////////////////////////////////////////////////////////////////////////

static volatile int stop;
void *region;

struct params {
    int uffd;
    long page_size;
};

static uint64_t addr_map_int[NUM_PAGES];
static int addr_map_machine[NUM_PAGES];
static struct in6_addr* addr_map_in6[NUM_PAGES];

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

// TODO: Change such that it always goto EXIT even on error (have a return val for the method)
static void *handler(void *arg) {
    struct params *param = arg;
    long page_size = param->page_size;

    // TODO: this needs to be changed so you don't redo what's done in main. Should be global (not sure if it's a good idea to be global)
    //       currently it works.

    // Setup network connections to server and directory server.
    int sockfd_server;
    struct addrinfo hints, *servinfo_server, *p_server;
    int rv;
    
    //Socket operator variables
    const int on=1, off=0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "0", &hints, &servinfo_server)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    p_server = bindSocket(p_server, servinfo_server, &sockfd_server);
    struct sockaddr_in6 *temp = (struct sockaddr_in6 *) p_server->ai_addr;
    temp->sin6_port = htons(strtol("5000", (char **)NULL, 10));
    genPacketInfo(sockfd_server);
    openRawSocket();

    // Set up timing arrays
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

    // Start handler
    int i = 0;

    for (;;) {
        struct uffd_msg msg;
        char *buf;

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
            handler_start[i] = getns();
            // printf("Handler start %d: %ld microseconds\n", i, handler_start[i]/1000);
            long long addr = msg.arg.pagefault.address;

            // fprintf(stdout, "Address: %"PRIx64"\n", (uint64_t) msg.arg.pagefault.address);
            uint64_t start_addr = (uint64_t) region;
            int index = (int) (addr - start_addr)/get_page_size();

            // fprintf(stdout, "Address: %"PRIx64", index: %d\n", (uint64_t) msg.arg.pagefault.address, index);

            if (addr_map_int[index] > 0 || addr_map_in6[index] != NULL) {
                // Do remote things, based on if we're simulating a centralized directory service or not.
                // 1. Get machine from the remote directory service server (Centralized directory service)
                //      a. Prepare packet, send to DS, receive IPv6 address back

                // 2. Get stuff from the remote server (SDN directory service)
                //      a. getRemoteMem() from client_lib.c
                struct in6_addr remoteMachine;
                remoteMachine = *addr_map_in6[index];

                char s[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &remoteMachine, s, sizeof(s));
                print_debug("Remote Machine is... %s\n", s);

                second_rtt_start[i] = getns();
                buf = getRemoteMem(sockfd_server, p_server, &remoteMachine);
                second_rtt_end[i] = getns();
            } else {
                perror("ERROR: not a remote address");
            }

            struct uffdio_copy copy;
            copy.src = (long long)buf;
            copy.dst = (long long)addr;
            copy.len = page_size;
            copy.mode = 0;
            if (ioctl(param->uffd, UFFDIO_COPY, &copy) == -1) {
                perror("ioctl/copy");
                free(buf);
                exit(1);
            }

            free(buf);

            handler_end[i] = getns();
            i++;
        }

    }

EXIT:

    print_times(first_rtt_start, first_rtt_end, second_rtt_start, second_rtt_end, handler_start, handler_end);

    freeaddrinfo(servinfo_server);
    close(sockfd_server);
    closeRawSocket();
    free(first_rtt_start);
    free(second_rtt_start);
    free(first_rtt_end);
    free(second_rtt_end);
    free(handler_start);
    free(handler_end);

    return NULL;
}

int isWord(char prev, char cur) {
    return isspace(cur) && isgraph(prev);
}

void usage() {
    printf("USAGE: ./userfaultfd_measure_pagefault [-l | -w | -d]\n");
    printf("\tWhere l, w, and d are optional flags:\n");
    printf("\t\tl: pages to local memory. If this flag isn't used, it will page to remote memory.\n");
    printf("\t\tw: writes page. If this flag isn't used, it will read the page.\n");
    printf("\t\td: simulates a centralized directory service. If this flag isn't used, it assumes the network is the directory service (i.e., SDN).\n");
}

int main(int argc, char **argv)
{
    // TODO: add usage
    if (argc > 4) {
        printf("ERROR: wrong number of arguments\n");
        usage();
        exit(1);
    }
    int uffd;
    long page_size;
    pthread_t uffd_thread;
    int local = 0, measure_write = 0, directService = 0;
    int c;
    opterr = 0; 

    while ((c = getopt(argc, argv, "lwd?")) != -1) {
        switch(c) {
            case 'l':
                local = 1;
                break;
            case 'w':
                measure_write = 1;
                break;
            case 'd':
                directService = 1;
                break;
            case '?':
                usage();
                break;
            default:
                printf("ERROR: unknown argument %c", c);
                usage();
                return -1;
        }
    }

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

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "0", &hints, &servinfo_server)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    p_server = bindSocket(p_server, servinfo_server, &sockfd_server);
    struct sockaddr_in6 *temp = (struct sockaddr_in6 *) p_server->ai_addr;
    temp->sin6_port = htons(strtol("5000", (char **)NULL, 10));
    genPacketInfo(sockfd_server);

    // Make every x regions remote
    // TODO: add line which writes a page full to the shared memory
    long i;
    char *cur = region;
    for (i = 0; i < NUM_PAGES; i++) {
        struct in6_addr * remotePointer = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
        struct in6_addr temp = allocateRemoteMem(sockfd_server, p_server);
        memcpy(remotePointer, &temp, IPV6_SIZE);
        addr_map_in6[i] = remotePointer;
    }

    if ((uffdio_register.ioctls & UFFD_API_RANGE_IOCTLS) !=
            UFFD_API_RANGE_IOCTLS) {
        fprintf(stderr, "unexpected userfaultfd ioctl set\n");
        exit(1);
    }

    struct stat fstat;
    stat("hound_of_baskerville_small.txt", &fstat);
    int blocksize = (int) fstat.st_blksize;

    char *buf = (char*) aligned_alloc(blocksize, page_size);

    if (buf == NULL) {
        printf("Null buffer");
        exit(1);
    }

    FILE *f;
    int fd = open("hound_of_baskerville_small.txt", O_DIRECT | O_SYNC);
    if (fd < 1) {
        perror("fd");
    }

    f = fdopen(fd, "r");
    if (f == NULL) {
        perror("open hound_of_baskerville_small.txt");
        exit(1);
    }

    int size = (int) fstat.st_size;

    // Write the story into the remote memory
    for (i = 0; i < NUM_PAGES; i++) {
        struct in6_addr remotePointer = *addr_map_in6[i];
        if (fseek(f, i*page_size, SEEK_SET) != 0) {
            perror("fseek offset");
        }

        if (fread(buf, blocksize, 1, f) <= 0) {
            perror("fread");
            exit(1);
        }
        writeRemoteMem(sockfd_server, p_server, buf, &remotePointer);
    }

    if (fclose(f)) {
        perror("fclose");
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

    int count = 0, index;
    char prev = ' ';

    // TODO: change to be WC. 
    for (index = 0; index < size; index++) {
        // TODO: Should also handle \n (i.e. any whitespace)
        //       should also ensure that it's only a word if
        //       there was a non white space character before it.
        if (isWord(prev, cur[index])) {
            printf("It's a word! prev: %c, cur: %d\n", prev, cur[index]);
            count++;
        }
        prev = cur[index];
    }

    printf("Word count: %d\n", count);


    stop = 1;
    pthread_join(uffd_thread, NULL);

    if (ioctl(uffd, UFFDIO_UNREGISTER, &uffdio_register.range)) {
        fprintf(stderr, "ioctl unregister failure\n");
        // TODO: add a "goto" exit label which frees everything and returns the correct code
        return 1;
    }

    FILE *f_stat = fopen("total_times.csv", "w");
    if (f_stat == NULL) {
        printf("Error opening file!\n");
        // TODO: add a "goto" exit label which frees everything and returns the correct code
        exit(1);
    }

    fprintf(f_stat, "start (ns),end (ns)\n");

    for (i = 0; i < NUM_PAGES; i++) {
        fprintf(f_stat, "%llu,%llu\n", (unsigned long long) total_start[i], (unsigned long long) total_end[i]);
    }

    free(total_start);
    free(total_end);
    fclose(f_stat);
    munmap(region, page_size * NUM_PAGES);

    // TODO: add a "goto" exit label which frees everything and returns the correct code
    return 0;
}
