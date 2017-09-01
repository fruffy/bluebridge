#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netdb.h>            // struct addrinfo
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_UDP, INET6_ADDRSTRLEN
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <net/ethernet.h>
#include <ifaddrs.h>
#include <errno.h>            // errno, perror()
#include <sys/mman.h>
#include <sys/epoll.h>
#include <signal.h>
#include <poll.h>
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "config.h"

// Define some constants.
#define ETH_HDRLEN 14  // Ethernet header length
#define IP6_HDRLEN 40  // IPv6 header length
#define UDP_HDRLEN 8  // UDP header length, excludes data

int set_interface_ip(struct config *configstruct) {
    struct ifreq ifr;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    strncpy(ifr.ifr_name, configstruct->interface, IFNAMSIZ);
    struct sockaddr_in6* addr = (struct sockaddr_in6*)&ifr.ifr_addr;
    ifr.ifr_addr.sa_family = AF_INET6;
    memcpy(&addr->sin6_addr,&configstruct->src_addr, IPV6_SIZE);
    ioctl(fd, SIOCSIFADDR, &ifr);

    ioctl(fd, SIOCGIFFLAGS, &ifr);
    strncpy(ifr.ifr_name, configstruct->interface, IFNAMSIZ);
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

    ioctl(fd, SIOCSIFFLAGS, &ifr);
    close(fd);
    return 0;
}
int set_source_port(struct config *configstruct, int isServer, char *src_port) {
    int port = 0;
    if (isServer) {
        printf("Setting up server port...\n");
        port = atoi(configstruct->server_port);
    } else if (atoi(src_port)){
        port = atoi(src_port);
    }
    if (!port) {
        struct sockaddr_in6 sin;
        socklen_t len = sizeof(sin);
        int sd_temp;
        if ((sd_temp = socket (AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
            perror ("socket() failed to get socket descriptor for using ioctl() ");
            return EXIT_FAILURE;
        }
        if (getsockname(sd_temp, (struct sockaddr *)&sin, &len) == -1) {
            perror("getsockname");
            close(sd_temp);
            return EXIT_FAILURE;
        } else {
            configstruct->src_port = htons(sin.sin6_port);
            close(sd_temp);
        }
    } else {
        configstruct->src_port = htons(port);
    }
    return EXIT_SUCCESS;
}

#define DELIM "="
#define SEP ","
struct config configure_bluebridge(char *filename, int isServer) {
    struct config configstruct;

    printf("Opening...%s\n",filename );
    FILE *file = fopen (filename, "r");
    if (file != NULL) {
        char line[1024];
        int i = 0;
        while(fgets(line, sizeof(line), file) != NULL) {
            char *cfline;
            cfline = strstr((char *)line,DELIM);
            cfline = cfline + strlen(DELIM);
            cfline[strcspn(cfline, "\n")] = 0;
            if (i == 0){
                    memcpy(configstruct.interface,cfline,20);
            } else if (i == 1){
                char *token;
                token = strtok(cfline, SEP);
                int j = 0;
                while (token != NULL) {
                        inet_pton(AF_INET6, token, configstruct.hosts[j].s6_addr);
                        token = strtok (NULL, ",");
                        j++;
                    }
                configstruct.num_hosts = j;
            } else if (i == 2){
                    memcpy(configstruct.server_port,cfline,strlen(cfline));
            } else if (i == 3){
                    set_source_port(&configstruct, isServer,cfline);
            } else if (i == 4){
                    inet_pton(AF_INET6, cfline, configstruct.src_addr.s6_addr);
            } else if (i == 5){
                    configstruct.debug = atoi(cfline);
            }
            i++;
        }
        printf("Loading Config...\n");
        printf("*** Interface %s\n", configstruct.interface);
        printf("*** Server port %s\n", configstruct.server_port);
        printf("*** Client port %d\n", ntohs(configstruct.src_port));
        printf("*** Debug active? %d\n", configstruct.debug);
        printf("*** %d target hosts:\t ", configstruct.num_hosts);
        for (int j =0; j < configstruct.num_hosts; j++) {
            char s[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &configstruct.hosts[j], s, sizeof s);
            printf("%s ",s);
        }
        printf("\n");
        printf("Setting interface IP...\n");
        set_interface_ip(&configstruct);

        fclose(file);
        return configstruct;
    } else {
        perror("Could not find or open file!");
        exit(1);
    }
}
