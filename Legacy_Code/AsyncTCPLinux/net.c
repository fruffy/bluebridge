#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "net.h"

char* identify_interface(void)
{
    int ret;
    struct ifaddrs *ifaddr, *ifa;
    char *retip = NULL;

    if (getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "Error getting device addresses\n");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        char host_ip[NI_MAXHOST];

        if (ifa->ifa_addr == NULL)
            continue;

        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        ret = getnameinfo(ifa->ifa_addr,
                sizeof(struct sockaddr_in),
                host_ip, NI_MAXHOST,
                NULL, 0, NI_NUMERICHOST);

        if (ret != 0) {
            fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(ret));
            exit(EXIT_FAILURE);
        }

        if (strncmp(host_ip, IPRANGE_PREFIX, sizeof(IPRANGE_PREFIX)-1) == 0) {
            retip = strdup(host_ip);
            break;
        }
    }

    freeifaddrs(ifaddr);
    return retip;
}

int create_addr_struct(char *ip, int port, struct sockaddr_in *addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons((short)port);

    if (!ip) {
        addr->sin_addr.s_addr = INADDR_ANY;
    } else {
        if (!inet_pton(AF_INET, ip, &addr->sin_addr)) {
            return -1;
        }
    }

    return 0;
}

int connect_to_target(char *ip, int port, int local_port)
{
    struct sockaddr_in addr, local_addr;
    int sockfd, ret;

    ret = create_addr_struct(ip, port, &addr);
    if (ret) {
        perror("create remote addr failed");
        return -1;
    }

#ifdef CONN_UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
#endif /* CONN_UDP */
    if (sockfd < 0) {
        perror("socket call failed");
        return -1;
    }

    /* if a local port is specified for outgoing connections bind the socket */
    if (local_port) {

        ret = create_addr_struct(NULL, local_port, &local_addr);
        if (ret) {
            perror("create local addr failed");
            goto out;
        }

        ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                         &ret, sizeof(ret));
        if (ret) {
            perror("so_reuseaddr sockopt failed");
            goto out;
        }

        ret = bind(sockfd, (struct sockaddr *) &local_addr,
                   sizeof(struct sockaddr_in));
        if (ret) {
            errno = EBUSY;
            goto out;
        }
    }

    ret = connect(sockfd, (struct sockaddr *)&addr,
                  sizeof(struct sockaddr_in));
    if (ret) {
        perror("connect call failed");
        goto out;
    }

    return sockfd;

out:
    ret = errno;
    close(sockfd);
    errno = ret;
    return -1;
}

int create_listening_socket(char *ip, int port)
{
    int sockfd, ret;
    struct sockaddr_in addr;

    ret = create_addr_struct(ip, port, &addr);
    if (ret) {
        perror("create listening addr failed");
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket call failed");
        return -1;
    }

    ret = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                &ret, sizeof(ret))) {
        perror("so_reuseaddr sockopt failed");
        goto failure;
    }

    ret = bind(sockfd, (struct sockaddr *) &addr,
                sizeof(struct sockaddr_in));
    if (ret) {
        perror("bind to local port failed");
        goto failure;
    }

    ret = listen(sockfd, 5);
    if (ret) {
        perror("listen call failed");
        goto failure;
    }

    return sockfd;

failure:
    ret = errno;
    close(sockfd);
    errno = ret;
    return -1;
}

void set_socket_options(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK))
    {
        perror("cannot set non-blocking mode on socket\n");
        return;
    }

#ifndef CONN_UDP
#if 0
    /* Disable nagle */
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
                (void *) &one, sizeof(one)) < 0)
    {
        perror("setsockopt TCP_NODELAY");
        return;
    }
#endif
#endif /* CONN_UDP */
}

void add_to_epoll(int epollfd, int sockfd, int flags)
{
    struct epoll_event ev;

    ev.events = (flags == 0) ?
            (EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR) : flags;
    ev.data.fd = sockfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        perror("epoll_ctl: add to epoll");
        exit(EXIT_FAILURE);
    }
}

void modify_epoll(int epollfd, int sockfd, int flags)
{
    struct epoll_event ev;

    ev.events = (flags == 0) ?
            (EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR) : flags;
    ev.data.fd = sockfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) == -1) {
        perror("epoll_ctl: add to epoll");
        exit(EXIT_FAILURE);
    }
}
