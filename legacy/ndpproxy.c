/*  Copyright (C) 2012  Alexey Andriyanov (alan@al-an.info)
    Based on the source by P.D. Buchan (pdbuchan@yahoo.com)
    http://www.pdbuchan.com/rawsock/rawsock.html
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>           // close(), getopt(), daemon()
#include <string.h>           // strcpy, memset(), and memcpy()

#include <netinet/icmp6.h>
#include <netinet/in.h>       // IPPROTO_IPV6, IPPROTO_ICMPV6
#include <netinet/ip.h>       // IP_MAXPACKET (65535)
#include <netinet/ip6.h>      // struct ip6_hdr
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <net/if.h>           // struct ifreq
#include <netinet/if_ether.h> // struct ethhdr, ETH_P_IPV6
#include <bits/socket.h>      // structs msghdr and cmsghdr
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl. Here, we need SIOCGIFHWADDR

#include <sys/types.h>
#include <ifaddrs.h>          // getifaddrs(), freeifaddrs()
#include <pcap.h>

int sd; // socket descriptor
int ifindex;
struct ifreq ifr;
struct in6_addr source; // the link-local address of this machine

// Taken from <linux/ipv6.h>, also in <netinet/in.h>
struct in6_pktinfo {
	struct in6_addr ipi6_addr;
	int             ipi6_ifindex;
};

// Checksum function
unsigned short int checksum (unsigned short int *addr, int len)
{
	int nleft = len;
	int sum = 0;
	unsigned short int *w = addr;
	unsigned short int answer = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= sizeof (unsigned short int);
	}

	if (nleft == 1) {
		*(unsigned char *) (&answer) = *(unsigned char *) w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	answer = ~sum;

	return (answer);
}

int send_nd_nadvert (struct in6_addr *dst, struct in6_addr *target)
{
	static const int hoplimit = 255;
	struct msghdr msghdr;
	struct cmsghdr *cmsghdr1, *cmsghdr2;
	struct in6_pktinfo *pktinfo;
	struct iovec iov;
	struct sockaddr_in6 destination;
	char s[INET6_ADDRSTRLEN];

	struct {
		// IPv6 pseudo-header to compute the ICMPv6 checksum
		struct in6_addr psrc;
		struct in6_addr pdst;
		uint32_t pupper_len;
		uint8_t dummy[3];
		uint8_t pproto;

		// ICMPv6 NA packet
		struct nd_neighbor_advert icmp_hdr;
		struct nd_opt_hdr icmp_opt_hdr;
		uint8_t mac_addr[IFHWADDRLEN];
	} opkt; // TODO: __attribute__ ((packed));

	int icmp_len = sizeof (struct nd_neighbor_advert) + sizeof (struct nd_opt_hdr) + IFHWADDRLEN;
	inet_ntop(AF_INET6,dst, s, sizeof s);
	printf("Target %s\n",s);
	inet_ntop(AF_INET6,target, s, sizeof s);
	printf("Dest %s\n",s);

	memset (&opkt, 0, sizeof (opkt));
	opkt.psrc = source;
	opkt.pdst = *dst;
	opkt.pupper_len = htonl (icmp_len);
	opkt.pproto = IPPROTO_ICMPV6;
	opkt.icmp_hdr.nd_na_type = ND_NEIGHBOR_ADVERT;
	opkt.icmp_hdr.nd_na_code = 0;
	opkt.icmp_hdr.nd_na_target = *target;
	opkt.icmp_opt_hdr.nd_opt_type = ND_OPT_TARGET_LINKADDR;
	opkt.icmp_opt_hdr.nd_opt_len = 1;
	memcpy (&opkt.mac_addr, &ifr.ifr_addr.sa_data, IFHWADDRLEN);
	opkt.icmp_hdr.nd_na_cksum = checksum ((unsigned short int *) &opkt, sizeof (opkt));
	
	inet_ntop(AF_INET6,dst, s, sizeof s);
	printf("AddressDestFirst%s\n",s);
	
	// prepare destination
	memset (&destination, 0, sizeof (destination));
	memcpy (&destination.sin6_addr, dst, sizeof (struct in6_addr));
	destination.sin6_family = AF_INET6;

	inet_ntop(AF_INET6,&destination.sin6_addr, s, sizeof s);
	printf("AddressDestAfter%s\n",s);

	// Prepare msghdr for sendmsg().
	memset (&msghdr, 0, sizeof (msghdr));
	msghdr.msg_name = &destination;  // Destination IPv6 address as struct sockaddr_in6
	msghdr.msg_namelen = sizeof (destination);
	memset (&iov, 0, sizeof (iov));
	iov.iov_base = (unsigned char *) &opkt.icmp_hdr;  // Point msghdr to packet buffer
	iov.iov_len = icmp_len;
	msghdr.msg_iov = &iov;                 // scatter/gather array
	msghdr.msg_iovlen = 1;                // number of elements in scatter/gather array

	// Tell msghdr we're adding cmsghdr data to change hop limit and specify interface.
	// Allocate some memory for our cmsghdr data.
	int cmsglen = CMSG_SPACE (sizeof (int)) + CMSG_SPACE (sizeof (struct in6_pktinfo));
	if (NULL == (msghdr.msg_control = (unsigned char *) malloc (cmsglen * sizeof (unsigned char)))) {
		fprintf (stderr, "malloc() failed\n");
		return 0;
	}
	memset (msghdr.msg_control, 0, cmsglen);
	msghdr.msg_controllen = cmsglen;

	// Change hop limit to 255 as required for neighbor advertisement (RFC 4861).
	cmsghdr1 = CMSG_FIRSTHDR (&msghdr);
	cmsghdr1->cmsg_level = IPPROTO_IPV6;
	cmsghdr1->cmsg_type = IPV6_HOPLIMIT;  // We want to change hop limit
	cmsghdr1->cmsg_len = CMSG_LEN (sizeof (int));
	*((int *) CMSG_DATA (cmsghdr1)) = hoplimit;  // Copy pointer to int hoplimit

	// Specify source interface index and source IP address for this packet via cmsghdr data.
	cmsghdr2 = CMSG_NXTHDR (&msghdr, cmsghdr1);
	cmsghdr2->cmsg_level = IPPROTO_IPV6;
	cmsghdr2->cmsg_type = IPV6_PKTINFO;  // We want to specify interface here
	cmsghdr2->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));
	pktinfo = (struct in6_pktinfo *) CMSG_DATA (cmsghdr2);
	pktinfo->ipi6_ifindex = ifindex;
	pktinfo->ipi6_addr = source;
	inet_ntop(AF_INET6,&pktinfo->ipi6_addr, s, sizeof s);
	printf("AddressSource %s\n",s);
	inet_ntop(AF_INET6,&destination.sin6_addr, s, sizeof s);
	printf("AddressDest %s\n",s);
	int ret = 1;
	if (sendmsg (sd, &msghdr, 0) < 0) {
		perror ("sendmsg");
		ret = 0;
	}
	free (msghdr.msg_control);
	/*sleep(10); */
	return ret;
}
/*sendmsg(3, {msg_name(28)={sa_family=AF_INET6, sin6_port=htons(0), inet_pton(AF_INET6, "::", &sin6_addr), sin6_flowinfo=0, sin6_scope_id=0},
msg_iov(1)=[{"\210\0bV\0\0\0\0\0\0\0\0\1\1\0\0\0\0\0\0\0\0\0\0\2\1\342\263\202\375\245\264", 32}], msg_controllen=64,
[{cmsg_len=20, cmsg_level=SOL_IPV6, cmsg_type=52}, {cmsg_len=36, cmsg_level=SOL_IPV6, cmsg_type=50}], msg_flags=0}, 0) = -1 EINVAL (Invalid argument)
*/
/*sendmsg(3, {msg_name(28)={sa_family=AF_INET6, sin6_port=htons(0), inet_pton(AF_INET6, "0:0:101::", &sin6_addr), sin6_flowinfo=0, sin6_scope_id=0},
msg_iov(1)=[{"\210\0\231\262\0\0\0\0\0\0\0\0\1\2\0\0P\5B\1\0\0\0\0\2\1\372i\30e\223d", 32}], msg_controllen=64,
[{cmsg_len=20, cmsg_level=SOL_IPV6, cmsg_type=52}, {cmsg_len=36, cmsg_level=SOL_IPV6, cmsg_type=50}], msg_flags=0}, 0) = 32*/

int find_link_local_ip (char *ifname, struct in6_addr * result)
{
	int ret = 0;
	struct ifaddrs * list;
	if (0 != getifaddrs (&list)) {
		perror ("getifaddrs");
		return 0;
	}
	
	struct ifaddrs *i = list;
	while (i) {
		if (0 == strcmp (i->ifa_name, ifname)) {
			if (i->ifa_addr->sa_family == AF_INET6) {
				struct sockaddr_in6 * v6addr = (struct sockaddr_in6 *) i->ifa_addr;
				if (IN6_IS_ADDR_LINKLOCAL (&v6addr->sin6_addr)) {
					*result = v6addr->sin6_addr;
					ret = 1;
					break;
				}
			}
		}
		i = i->ifa_next;
	}
	
	freeifaddrs (list);
	return ret;
}
int find_non_local_ip (char *ifname, struct in6_addr * result)
{
	int ret = 0;
	struct ifaddrs * list;
	if (0 != getifaddrs (&list)) {
		perror ("getifaddrs");
		return 0;
	}
	
	struct ifaddrs *i = list;
	while (i) {
		if (0 == strcmp (i->ifa_name, ifname)) {
			if (i->ifa_addr->sa_family == AF_INET6) {
				struct sockaddr_in6 * v6addr = (struct sockaddr_in6 *) i->ifa_addr;
				if (IN6_IS_ADDR_LINKLOCAL (&v6addr->sin6_addr) == 0) {
					*result = v6addr->sin6_addr;
					ret = 1;
					break;
				}
			}
		}
		i = i->ifa_next;
	}
	
	freeifaddrs (list);
	return ret;
}

void display_help (void) {
	printf ("\
ndp-proxy [ OPTIONS ] -i IFACE [ PREFIX [ ... ] ]\n\
  PREFIX      an IPv6 prefix (or multiple) limiting target ranges\n\
  -i IFACE    interface name to listen for neighbor solicitations on\n\
  -p PIDFILE  create the pid file\n\
  -h          show this help message\n\
  -v          display version info\n\
");
}

void display_version (void) {
	printf ("\
ndp-proxy 0.2\n\
");
}

struct prefix_list {
	struct prefix_list *next;
	struct in6_addr addr;
	unsigned int mask;
};

int addr_matches_filter (struct prefix_list *filter, struct in6_addr *addr) {
	while (filter) {
		int i;
		int found = 1;
		uint32_t mask;
		uint32_t *a = (uint32_t *) &filter->addr;
		uint32_t *b = (uint32_t *) addr;
		for (i = 1; i <= 4; i++) {
			// calculate binary mask
			if (i * 32 <= filter->mask)
				mask = 0xFFFFFFFF;
			else if ((i - 1) * 32 >= filter->mask)
				mask = 0;
			else
				mask = htonl (0xFFFFFFFF << (32 - (filter->mask % 32)));

			if ((*a & mask) != (*b & mask)) {
				found = 0;
				break;
			}
			a++; b++;
		}
		if (found)
			return 1;
		filter = filter->next;
	}
	return 0;
}

int main (int argc, char **argv)
{
	struct nd_neighbor_solicit *ns;
	struct sockaddr_in6 src;
	struct prefix_list * filters = NULL;
	FILE *pid_file;

	memset (&ifr, 0, sizeof (ifr));

	int opt;
	while ((opt = getopt (argc, argv, "hvi:p:")) != -1) {
		int do_break = 0;
		switch (opt) {
			case 'h':
				display_help();
				return EXIT_SUCCESS;
			case 'v':
				display_version();
				return EXIT_SUCCESS;
			case 'i':
				strncpy (ifr.ifr_name, optarg, sizeof (ifr.ifr_name) - 1);
				break;
			case 'p':
				pid_file = fopen (optarg, "w");
				if (NULL == pid_file) {
					fprintf (stderr, "fopen %s: ", optarg);
					perror (NULL);
					return EXIT_FAILURE;
				}
				break;
			default: /* '?' */
				do_break = 1;
				break;
		}
		if (do_break)
			break;
	}

	if (*ifr.ifr_name == '\0') {
		fprintf (stderr, "Expecting interface name as argument. Use -h to get help.\n");
		return EXIT_FAILURE;
	}
	struct in6_addr v6buff;
	unsigned int mask;
	char addr[BUFSIZ];
	for (; optind < argc; optind++) {
		if (2 != sscanf (argv[optind], "%[^/]/%u", (char*)&addr, &mask)) {
			fprintf (stderr, "Invalid IPv6 prefix `%s'\n", argv[optind]);
			return EXIT_FAILURE;
		}
		if (mask > 128) {
			fprintf (stderr, "Invalid IPv6 prefix length %d\n", mask);
			return EXIT_FAILURE;
		}
		printf("Address1 %s\n",addr);
		switch (inet_pton (AF_INET6, addr, &v6buff))
		{
			case -1:
				perror ("inet_pton");
				return EXIT_FAILURE;
			case 0:
				fprintf (stderr, "Invalid IPv6 address `%s'\n", addr);
				return EXIT_FAILURE;
		}
		struct prefix_list * new_item = malloc (sizeof (struct prefix_list));
		if (NULL == new_item) {
			fprintf (stderr, "malloc() failed\n");
			return EXIT_FAILURE;
		}
		char s[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6,&v6buff, s, sizeof s);
		printf("Address2 %s\n",s);
		new_item->addr = v6buff;
		new_item->mask = mask;
		new_item->next = filters;
		filters = new_item;
	}

	if (! find_link_local_ip (ifr.ifr_name, &source)) {
		fprintf (stderr, "Unable to obtain IPv6 link-local address on %s\n", ifr.ifr_name);
		return EXIT_FAILURE;
	}
/*	if (! find_link_local_ip (ifr.ifr_name, &source)) {
		fprintf (stderr, "Unable to obtain IPv6 link-local address on %s\n", ifr.ifr_name);
		return EXIT_FAILURE;
	}*/
	// Request a raw socket descriptor sd.
	if ((sd = socket (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		perror ("socket");
		return EXIT_FAILURE;
	}

	// Bind socket to specified interface
	if (setsockopt (sd, SOL_SOCKET, SO_BINDTODEVICE, (void *) &ifr, sizeof (ifr)) < 0) {
		perror ("SO_BINDTODEVICE");
		return EXIT_FAILURE;
	}
		printf("check3 \n");

	// Retrieve source interface index.
	if ((ifindex = if_nametoindex (ifr.ifr_name)) == 0) {
		fprintf (stderr, "if_nametoindex %s: ", ifr.ifr_name);
		perror (NULL);
		return EXIT_FAILURE;
	}

	// Obtain advertising node (source) MAC address.
	if (ioctl (sd, SIOCGIFHWADDR, &ifr) < 0) {
		perror ("ioctl() failed to get MAC address of advertising node ");
		return (EXIT_FAILURE);
	}

	// open pcap handler for device
	char errbuff[PCAP_ERRBUF_SIZE];
	pcap_t * ph = pcap_open_live (ifr.ifr_name, IP_MAXPACKET, 1, 0, errbuff); // promisc=1, timeout=0
	if (NULL == ph) {
		fprintf (stderr, "pcap_open_live: %s\n", errbuff);
		return EXIT_FAILURE;
	}
	
	// filter only ICMPv6 messages
	struct bpf_program fp;
	const char * filter_exp = "ip6 and icmp6";
	if (-1 == pcap_compile (ph, &fp, filter_exp, 1, 0)) {
		fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr (ph));
		return EXIT_FAILURE;
	}
	if (pcap_setfilter (ph, &fp) == -1) {
		fprintf (stderr, "pcap_setfilter `%s': %s\n", filter_exp, pcap_geterr (ph));
		return EXIT_FAILURE;
	}
	printf("INFO: Disabled demon mode.\n");
/*	if (-1 == daemon(0, 0)) {
		printf("daeomon\n");
		perror ("daemon");
		return EXIT_FAILURE;
	}*/
	if (pid_file) {
		fprintf (pid_file, "%d\n", getpid());
		fclose (pid_file);
	}
	const u_char * packet;		/* The actual packet */
	struct pcap_pkthdr * header;	/* The header that pcap gives us */
	int pcap_ret = 1;
	while (1 == (pcap_ret = pcap_next_ex (ph, &header, &packet)))
	{
		int len = sizeof (struct ethhdr);

		if (header->caplen < len)
			continue;
		struct ethhdr * hdr = (struct ethhdr *) packet;
		if (ntohs (hdr->h_proto) != ETH_P_IPV6)
			continue;
		len += sizeof (struct ip6_hdr);
		if (header->caplen < len)
			continue;
		struct ip6_hdr * hdr2 = (struct ip6_hdr *) (hdr + 1);
		if (hdr2->ip6_nxt != IPPROTO_ICMPV6)
			continue;
		len += sizeof (struct icmp6_hdr);
		if (header->caplen < len)
			continue;
		struct icmp6_hdr * hdr3 = (struct icmp6_hdr *) (hdr2 + 1);
		if (hdr3->icmp6_type != ND_NEIGHBOR_SOLICIT)
			continue;
		len += sizeof (struct nd_neighbor_solicit) - sizeof (struct icmp6_hdr);
		if (header->caplen < len)
			continue;
		struct nd_neighbor_solicit * ns = (struct nd_neighbor_solicit * ) hdr3;
		char s[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6,&hdr2->ip6_src, s, sizeof s);
		printf("Whats going on here %s\n",s);
		if (! filters || addr_matches_filter (filters, &ns->nd_ns_target))
			inet_ntop(AF_INET6,&hdr2->ip6_src, s, sizeof s);
			printf("Whats going on here2 %s\n",s);
/*			if (strcmp(s,"::") == 0) {
				int temp = 1;
				struct in6_addr * test = &hdr2->ip6_src;
				memcpy(test->s6_addr+15, &temp,1);
			}
			inet_ntop(AF_INET6,&hdr2->ip6_src, s, sizeof s);
			printf("Whats going on here3 %s\n",s);*/

			if (! send_nd_nadvert (&hdr2->ip6_src, &ns->nd_ns_target))
				break;
	}

	if (pcap_ret == -1)
		pcap_perror (ph, "pcap_next_ex");

	close (sd);
	pcap_close(ph);
	
	// free filters list
	while (filters) {
		struct prefix_list * tmp = filters;
		filters = filters->next;
		free (tmp);
	}

	return EXIT_FAILURE; // daemon is supposed to work inifinitely
}