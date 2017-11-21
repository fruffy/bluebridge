/** Key-value datastore*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>

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

#define MAX_PIPELINE    512
#define MAX_REQUESTS    4000000
#define MAX_EVENTS      512

/* Data for tx/rx */
char key[KEY_SIZE] = {0};

/* CLI parameters */
static int port = DEFAULT_PORT;
static int local_port = 0;
static int server_threads = 1;
static char *frontend_ip = NULL;
static int runtime = 33;
static uint64_t total_reqs = MAX_REQUESTS;
static uint64_t pipeline = MAX_REQUESTS;
static int num_threads = 1;
static int pin_threads = 0;
static int key_size = KEY_SIZE;
static int value_size = MAX_VALUE_SIZE;

__thread struct timespec ts_ring[MAX_PIPELINE];
__thread uint64_t req = 0;
__thread uint64_t resp = 0;
__thread uint64_t lat_sum = 0;
__thread int ts_prod = 0;
__thread int ts_cons = 0;

volatile int quit = 0;

static inline uint64_t timediff(struct timespec *start, struct timespec *end)
{
    return ((uint64_t) (end->tv_sec - start->tv_sec) * 1E6) +
        (end->tv_nsec - start->tv_nsec) / 1E3;
}

static int client_recv_resp(int fd, char* resp_buf, int *offset)
{
    int ret;
    while ((ret = read_value(resp_buf, offset)) == RES_DONE) {
        struct timespec *then;
        struct timespec now;
        uint64_t delta;

        assert(ts_cons != ts_prod);

        clock_gettime(CLOCK_MONOTONIC, &now);
        resp++;

        then = &ts_ring[ts_cons];
        ts_cons = (ts_cons + 1) & (MAX_PIPELINE - 1);
        delta = timediff(then, &now);
        lat_sum += delta;

        //fprintf(stderr, "%s\n", __FUNCTION__);
    }
    return ret;
}

static int client_send_req(int fd, char* payload, int *offset)
{
    int ret;

    if ((ret = write_key(payload, offset)) == RES_DONE) {
        clock_gettime(CLOCK_MONOTONIC, &ts_ring[ts_prod]);
        ts_prod = (ts_prod + 1) & (MAX_PIPELINE - 1);
        req++;
    }

    //fprintf(stderr, "%s\n", __FUNCTION__);
    return ret;
}

void* client_thread(void *param)
{
    struct epoll_event events[MAX_EVENTS];
    struct drand48_data local_buf, read_buf;
    struct timeval start, end;
    char response[MAX_VALUE_SIZE] = {0};
    double *tps;

    int client_to_server = -1;
    int epollfd = -1;
    int nfds = -1;
    int n = -1;
    int err = -1;
    int read_offset = 0;
    int write_offset = 0;
    int thread_num = (int) (uint64_t) param;
    int rport = port;

    tps = (double *) malloc (sizeof(double));

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

    srand48_r(time(0), &local_buf);
    sleep(2);
    srand48_r(time(0), &read_buf);

    if (server_threads > 1) {
        rport = port + (thread_num % server_threads);
    }

    client_to_server = connect_to_target(frontend_ip, rport, local_port);
    if (client_to_server < 0) {
        perror("Failed to connect to server");
        exit(-1);
    }
    set_socket_options(client_to_server);
    add_to_epoll(epollfd, client_to_server, EPOLLIN|EPOLLOUT);

    printf("Starting client %d/%d, pipeline: %lu/%lu\n",
            thread_num+1, num_threads, pipeline, total_reqs);
    printf("Connected to %s:%d\n", frontend_ip, rport);

    gettimeofday(&start, 0);
    while (!quit) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 5);

        if (nfds == -1) {
            if (errno == EINTR) {
                break;
            }
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (n = 0; n < nfds; ++n) {
            /* Handle errors first */
            if (events[n].events & (EPOLLRDHUP|EPOLLERR)) {
                epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, 0);

                close(events[n].data.fd);
                if (events[n].data.fd == client_to_server) {
                    fprintf(stderr, "server disconnected\n");
                    client_to_server = -1;
                    quit = 1;
                    break;
                }
            }

            /* Space available to send data */
            if ((req < total_reqs)
                && (events[n].events & EPOLLOUT)
                && (req - resp < pipeline)) {

                int count = 0;
                err = RES_DONE;

                while ((req < total_reqs)
                       && (req - resp < pipeline)
                       && (err == RES_DONE)) {

                    err = client_send_req(events[n].data.fd,
                                          key, &write_offset);
                    if (err == RES_FAIL) {
                        fprintf(stderr, "Cannot send request to server: %s\n",
                                strerror(errno));
                        quit = 1;
                        break;
                    }
                    count++;
                }
            }

            /* Incoming data */
            if (events[n].events & EPOLLIN) {

                err = client_recv_resp(events[n].data.fd, response, &read_offset);
                if (err == RES_FAIL) {
                    fprintf(stderr, "Cannot recv response from server: %s\n",
                            strerror(errno));
                    quit = 1;
                    break;
                }
            }

            /* requests sent ... */
            if (req == total_reqs) {
                modify_epoll(epollfd, events[n].data.fd, EPOLLIN);
            }

            /* done ... */
            if (resp >= total_reqs) {
                quit = 1;
                break;
            }
        }
    }

    gettimeofday(&end, 0);

    *tps = resp * 1000000.0 / getusdiff(&start, &end);
    printf("%d clients:%lu requests, %lu responses @ %.0f trans/s (%lu us)\n",
            num_threads, req, resp, *tps, lat_sum / resp);
    fflush(stdout);

    if (client_to_server > 0) {
        shutdown(client_to_server, SHUT_RDWR);
    }
    close(epollfd);

    pthread_exit((void *)tps);
}

static void sighandler(int signal)
{
    quit = 1;
    __sync_synchronize();
}

static void usage()
{
    fprintf(stderr, "asyncclient\n\t"
            "-h <server ip>\n\t"
            "-H <server port (default 9876)>\n\t"
            "-P <local port to connect to server [default is random pick]>\n\t"
            "-d <maximum runtime (seconds) default 33>\n\t"
            "-v <value size [<= 4096 (default)]>\n\t"
            "-n <number of threads/cores default 1>\n\t"
            "-N <number of server cores default 1>\n\t"
            "-T <total number of requests to send (default 4000000)>\n\t"
            "-t <simultaneous pipeline requests (default=all)>\n\t"
            "-c <pin threads (default=off)>\n");
    return;
}

static void parse_args(int argc, char **argv)
{
    char c;

    while((c = getopt(argc, argv, "h:H:P:d:n:N:t:T:v:c")) != -1)
    {
        switch(c) {
            case 'h':
                frontend_ip = optarg;
                break;

            case 'H':
                port = atoi(optarg);
                break;

            case 'P':
                local_port = atoi(optarg);
                break;

            case 'd':
                runtime = atoi(optarg);
                break;

            case 'n':
                num_threads = atoi(optarg);
                break;

            case 'N':
                server_threads = atoi(optarg);
                break;

            case 't':
                pipeline = atoll(optarg);
                break;

            case 'T':
                total_reqs = atoll(optarg);
                break;

            case 'c':
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

    if (pipeline > total_reqs) {
        pipeline = total_reqs;
    }

    if (!frontend_ip || (value_size > MAX_VALUE_SIZE)) {
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
    int i;
    double total_tps = 0;
    double bw;

    strcpy(key, "local-key");
    parse_args(argc, argv);

    /* Thread identifiers */
    thread_ids = malloc(num_threads * sizeof(pthread_t));

    /* Set signal handlers */
    register_signal_handlers(sighandler);
    alarm(runtime);

    for (i = 0; i < num_threads; i++) {
        pthread_create(&thread_ids[i], NULL, client_thread, (void *)(uint64_t)i);
    }

    for (i = 0; i < num_threads; i++) {
        double *thread_tps;

        pthread_join(thread_ids[i], (void **)&thread_tps);
        total_tps += *thread_tps;
        free(thread_tps);
    }

    bw = (total_tps * value_size* 8) / (1000 * 1000 * 1000);
    fprintf(stderr, "Shutting down..\n");
    printf("Cumulative tps: %.0f trans/s (%.2f Gbps)\n", total_tps, bw);
    fflush(stdout);

    free(thread_ids);

    return 0;
}
