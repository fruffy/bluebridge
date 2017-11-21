/** Key-value datastore*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "proc.h"
#include "net.h"

#include "config.h"

#define MAX_EVENTS              512
#define MAX_STATIC_SOCKETS      1024

#define RPC_HEADER              8

#define read_struct(p, of)  (rw_exact(fd, OP_READ,  (char *) (p), sizeof((*p)), of))
#define write_struct(p, of) (rw_exact(fd, OP_WRITE, (char *) (p), sizeof((*p)), of))

/* Data for tx/rx */
static char local_key[KEY_SIZE] = {0};
static char local_value[MAX_VALUE_SIZE] = {0};

/* CLI Parameters */
static char *self_ip    = NULL;
static int self_port    = DEFAULT_PORT;

static int num_threads = 1;
static int pin_threads = 0;

static int key_size = KEY_SIZE;
static int value_size = MAX_VALUE_SIZE;

static volatile int quit = 0;

struct socket_metadata {
    int fd;

    int r_offset;
    int w_offset;
    int pending;

    /* wastes some space, but should never need more than this */
    char r_buf[MAX_VALUE_SIZE + RPC_HEADER];
    char w_buf[MAX_VALUE_SIZE + RPC_HEADER];

};

struct socket_metadata *g_sockets;

static int allocate_socket_metadata(struct socket_metadata *sockets, int sockfd)
{
    struct socket_metadata *new_sock = &sockets[sockfd];

    memset(new_sock, 0, sizeof(struct socket_metadata));
    new_sock->fd = sockfd;

    return 0;
}

static void disconnect_socket(struct socket_metadata *sockets, int epollfd, int sockfd)
{
    fprintf(stderr, "Disconnecting socket %d\n", sockfd);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

static int client_recv_req(int fd, struct socket_metadata *cl_sock)
{
    int ret;

    while ((ret = read_key(cl_sock->r_buf, &cl_sock->r_offset)) == RES_DONE) {

        if (strcmp(cl_sock->r_buf, local_key) == 0) {
            cl_sock->pending++;
        }
    }

    return ret;
}

static int client_send_resp(int fd, struct socket_metadata *cl_sock)
{
    int ret;

    while (cl_sock->pending &&
           (ret = write_value(local_value, &cl_sock->w_offset)) == RES_DONE) {

        cl_sock->pending--;
    }

    return ret;
}

/* This is fairly hacky: each thread listens on a different port (and
 * connects to the corresponding port on the backplane). This is because,
 * having each thread listen/accept on the same port number is a
 * bad idea on Linux. The client can round-robin over port numbers to try
 * and approximate a partitioned network stack.
 */
void* server_thread(void *param)
{
    struct epoll_event events[MAX_EVENTS];
    struct socket_metadata *client_sockets = NULL;
    int listen_sock = -1;
    int epollfd = -1;
    int nfds = -1;
    int n = -1;
    int err = -1;
    int thread_num = (int) (uint64_t) param;
    int listen_port = self_port + thread_num;

    if (pin_threads) {
        fprintf(stderr, "Pinning thread %d to core %d\n",
                thread_num, thread_num);
        pin_thread(pthread_self(), thread_num);
    }

    /* Set up event handling */
    epollfd = epoll_create(MAX_EVENTS);
    if (epollfd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }
    memset(events, 0, sizeof(struct epoll_event) * MAX_EVENTS);

    listen_sock = create_listening_socket(self_ip, listen_port);
    if (listen_sock < 0) {
        perror("Failed to create listening socket");
        goto out;
    }
    set_socket_options(listen_sock);
    add_to_epoll(epollfd, listen_sock, EPOLLIN);
    printf("Listening on %s:%d\n", self_ip, listen_port);

    client_sockets = g_sockets;

    while (!quit) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 1);

        if (nfds == -1) {
            if (errno == EINTR) {
                break;
            }
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (n = 0; n < nfds; ++n) {
            int cur_sockfd = events[n].data.fd;
            struct socket_metadata *cur_sock_meta;

            if (cur_sockfd == listen_sock
                && (events[n].events & EPOLLIN)) {

                int conn_sock;
                struct sockaddr_in client = {0};
                socklen_t client_addrlen = sizeof(struct sockaddr_in);
                char *accept_ip;

                conn_sock = accept(listen_sock, (struct sockaddr *) &client,
                                   &client_addrlen);
                if (conn_sock == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                accept_ip = inet_ntoa(client.sin_addr);

                fprintf(stderr, "Accepting connection (%d) from %s:%d\n",
                        conn_sock, accept_ip, ntohs(client.sin_port));

                if (conn_sock >= MAX_STATIC_SOCKETS) {
                    fprintf(stderr, "Socket fd exceeds metadata: %d\n", conn_sock);
                    exit(EXIT_FAILURE);
                }

                err = allocate_socket_metadata(client_sockets, conn_sock);
                if (err) {
                    perror("allocate_socket_metadata");
                    exit(EXIT_FAILURE);
                }

                set_socket_options(conn_sock);
                add_to_epoll(epollfd, conn_sock, 0);

                continue;
            }

            cur_sock_meta = &client_sockets[cur_sockfd];
            if (cur_sock_meta->fd != cur_sockfd) {
                fprintf(stderr, "No metadata for socket %d\n", cur_sockfd);
                exit(EXIT_FAILURE);
            }

            /* Handle errors first */
            if (events[n].events & (EPOLLRDHUP|EPOLLERR)) {
                disconnect_socket(client_sockets, epollfd, cur_sockfd);
            }

            /* Space available to send data */
            if (cur_sock_meta->pending
                && (events[n].events & EPOLLOUT)) {

                err = client_send_resp(cur_sockfd, cur_sock_meta);

                if (err == RES_FAIL) {
                    continue;
                }
            }

            /* Incoming data */
            if (events[n].events & EPOLLIN) {

                err = client_recv_req(cur_sockfd, cur_sock_meta);

                if (err == RES_FAIL) {
                    continue;
                }
            }
        }
    }

out:
    close(listen_sock);
    close(epollfd);
    return NULL;
}

static void sighandler(int signal)
{
    quit = 1;
    __sync_synchronize();
}

static void usage()
{
    fprintf(stderr, "triangle-server\n\t"
            "-s <ip of interface to listen on>\n\t"
            "-S <listening port [defaults to 9876 (front) or 8888 (back)]>\n\t"
            "-v <value size [<= 4096 (default)]>\n\t"
            "-t <pin threads [default is off]>\n\t"
            "-n <number of threads [default is 1]>\n");

    return;
}

void parse_args(int argc, char **argv)
{
    char c;

    while((c = getopt(argc, argv, "s:S:n:v:t")) != -1)
    {
        switch(c) {
            case 's':
                self_ip = optarg;
                break;

            case 'S':
                self_port = atoi(optarg);
                break;

            case 'n':
                num_threads = atoi(optarg);
                break;

            case 't':
                pin_threads = 1;
                break;

            case 'v':
                value_size = atoi(optarg);
                break;

            default:
                fprintf(stderr, "unrecognized option %s\n", optarg);
                goto exit_bad;
        }
    }

    if (self_port < 1024 || value_size > MAX_VALUE_SIZE) {
        goto exit_bad;
    }

    return;

exit_bad:
    usage();
    exit(-1);
}

int main(int argc, char *argv[])
{
    pthread_t *thread_ids;
    int free_str = 0;
    int i;

    strcpy(local_key, "local-key");
    strcpy(local_value, "local-value");

    parse_args(argc, argv);

    if (self_ip == NULL) {
        self_ip = identify_interface();
        if (self_ip == NULL) {
            perror("identify_interface");
            exit(EXIT_FAILURE);
        }
        free_str = 1;
    }

    /* Thread identifiers */
    thread_ids = malloc(num_threads * sizeof(pthread_t));

    /* Common global sockets */
    g_sockets = (struct socket_metadata *) malloc(
                MAX_STATIC_SOCKETS * sizeof(struct socket_metadata));
    if (!g_sockets) {
        perror("Failed to allocate socket buffer\n");
        goto out;
    }
    memset(g_sockets, 0, sizeof(struct socket_metadata) * MAX_STATIC_SOCKETS);

    /* Set signal handlers */
    register_signal_handlers(sighandler);

    for (i = 0; i < num_threads; i++) {
        pthread_create(&thread_ids[i], NULL, server_thread,
                (void *) (uint64_t) i);
    }

    for (i = 0; i < num_threads; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    if (free_str) {
        free(self_ip);
    }

out:
    free(g_sockets);
    free(thread_ids);

    return 0;
}
