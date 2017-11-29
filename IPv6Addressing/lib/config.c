#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <net/if.h>           // struct ifreq
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "config.h"

// Define some constants.
#define ETH_HDRLEN 14  // Ethernet header length
#define IP6_HDRLEN 40  // IPv6 header length
#define UDP_HDRLEN 8  // UDP header length, excludes data

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

struct in6_ifreq {
    struct in6_addr ifr6_addr;
    __u32 ifr6_prefixlen;
    unsigned int ifr6_ifindex;
};
struct config bb_config;

int set_interface_ip(struct config *config) {
    struct ifreq ifr;
    struct in6_ifreq ifr6;
    int fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP);

    strncpy(ifr.ifr_name, config->interface, IFNAMSIZ);
    //struct sockaddr_in6* addr = (struct sockaddr_in6*) &ifr.ifr6_addr;
    ifr.ifr_addr.sa_family = AF_INET6;
    //memcpy(&addr->sin6_addr, &config->src_addr, IPV6_SIZE);
    if(ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("SIOCGIFHWADDR");
    }
    if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("SIOCGIFFLAGS");
    }
    if (ioctl(fd, SIOGIFINDEX, &ifr) < 0) {
        perror("SIOGIFINDEX");
    }
    memcpy(&ifr6.ifr6_addr, &config->src_addr,
               sizeof(struct in6_addr));
    ifr6.ifr6_ifindex = ifr.ifr_ifindex;
    ifr6.ifr6_prefixlen = 64;
    if (ioctl(fd, SIOCSIFADDR, &ifr6) < 0) {
        perror("SIOCSIFADDR");
    }
    strncpy(ifr.ifr_name, config->interface, IFNAMSIZ);
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    if(ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) {
        perror("SIOCSIFFLAGS");
    }
    ifr.ifr_mtu = 9000;
    if(ioctl(fd, SIOCSIFMTU, &ifr) < 0) {
        perror("SIOCSIFMTU");
    } 
    close(fd);
    return EXIT_SUCCESS;
}

int set_source_port(struct config *config, int isServer, char *src_port) {
    int port = 0;
    if (isServer) {
        printf("Setting up server port...\n");
        port = atoi(config->server_port);
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
            config->src_port = htons(sin.sin6_port);
            close(sd_temp);
        }
    } else {
        config->src_port = htons(port);
    }
    return EXIT_SUCCESS;
}

#define DELIM "="
#define SEP ","
struct config set_bb_config(char *filename, int isServer) {
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
                    memcpy(bb_config.interface,cfline,20);
            } else if (i == 1){
                char *token;
                token = strtok(cfline, SEP);
                int j = 0;
                while (token != NULL) {
                        inet_pton(AF_INET6, token, bb_config.hosts[j].s6_addr);
                        token = strtok (NULL, ",");
                        j++;
                    }
                bb_config.num_hosts = j;
            } else if (i == 2){
                    strncpy(bb_config.server_port,cfline,strlen(cfline));
            } else if (i == 3){
                    set_source_port(&bb_config, isServer,cfline);
            } else if (i == 4){
                    inet_pton(AF_INET6, cfline, bb_config.src_addr.s6_addr);
            } else if (i == 5){
                    bb_config.debug = atoi(cfline);
            }
            i++;
        }
        printf("Loading Config...\n");
        printf("*** Interface %s\n", bb_config.interface);
        printf("*** Server port %s\n", bb_config.server_port);
        printf("*** Client port %d\n", ntohs(bb_config.src_port));
        printf("*** Debug active? %d\n", bb_config.debug);
        printf("*** %d target hosts:\t ", bb_config.num_hosts);
        for (int j =0; j < bb_config.num_hosts; j++) {
            char s[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &bb_config.hosts[j], s, sizeof s);
            printf("%s ",s);
        }
        printf("\n");
        printf("Setting interface IP...\n");
        set_interface_ip(&bb_config);
        fclose(file);
        return bb_config;
    } else {
        perror("Could not find or open file!");
        exit(1);
    }
}

struct config get_bb_config(){
    return bb_config;
}
