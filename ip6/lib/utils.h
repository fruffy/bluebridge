#ifndef PROJECT_UTILS
#define PROJECT_UTILS

#include <time.h>
#include <assert.h>
#include <netdb.h>            // struct addrinfo

#define DEBUG 0

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define RESET "\033[0m"

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

int print_bytes(char *receiveBuffer);

int print_n_bytes(void *receiveBuffer, int num);

int print_n_chars(void *receiveBuffer, int num);

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
