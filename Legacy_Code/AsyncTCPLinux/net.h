#ifndef _NET_H
#define _NET_H

#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

#define IPRANGE_PREFIX  "192.168"

#define OP_READ         1
#define OP_WRITE        2
#define RES_DONE        1
#define RES_WAIT        0
#define RES_FAIL       -1

#define read_key(p, of)  (rw_exact(fd, OP_READ, (p), key_size, of))
#define write_key(p, of) (rw_exact(fd, OP_WRITE, (p), key_size, of))

#define read_value(p, of)  (rw_exact(fd, OP_READ, (p),  value_size, of))
#define write_value(p, of) (rw_exact(fd, OP_WRITE, (p), value_size, of))

char* identify_interface(void);

int create_addr_struct(char *ip, int port, struct sockaddr_in *addr);
int connect_to_target(char *ip, int port, int local_port);
int create_listening_socket(char *ip, int port);
void set_socket_options(int sock);

void add_to_epoll(int epollfd, int sockfd, int flags);
void modify_epoll(int epollfd, int sockfd, int flags);

static inline long int getusdiff(struct timeval *start, struct timeval *end)
{
    return ((end->tv_sec - start->tv_sec) * 1000000) +
            (end->tv_usec - start->tv_usec);
}

static inline int rw_exact(int fd, int op,
        char *data, int size, int *offset)
{
    int e;

    while (1) {

        if (op == OP_READ) {
            e = read(fd, data + *offset, size - *offset);
        } else {
            e = write(fd, data + *offset, size - *offset);
        }

        if (e > 0) {
            *offset += e;
            if (*offset == size) {
                *offset = 0;
                return RES_DONE;
            }
        } else {
            return (errno == EAGAIN) ? RES_WAIT : RES_FAIL;
        }
    }
}

#endif /* _NET_H */
