#ifndef PROJECT_UTILS
#define PROJECT_UTILS

#include <time.h>
#include <assert.h>
#include <netdb.h>            // struct addrinfo

#define DEBUG 1

#define print_debug(...); \
    if (DEBUG) {                     \
        printf("[DEBUG]: ");         \
        printf(__VA_ARGS__);     \
        printf("\n");                \
    }                                \

/// (unimportant) macro for loud failure
#define RETURN_ERROR(lvl, msg) \
  do {                    \
    perror(msg); \
    return lvl;            \
  } while(0);

//void print_debug(char* message);

int printBytes(char *receiveBuffer);

int printNBytes(char *receiveBuffer, int num);

int print_addrInfo(struct addrinfo *result);

int getLine(char *prmpt, char *buff, size_t sz);
unsigned char *gen_rdm_bytestream(size_t num_bytes);

static inline uint64_t getns(void)
{
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(ret == 0);
    return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}


#endif
