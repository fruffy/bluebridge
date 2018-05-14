[![DOI](https://zenodo.org/badge/84783444.svg)](https://zenodo.org/badge/latestdoi/84783444)


# BlueBridge
An (aspiring) distributed shared memory system intended to simplify large-scale computing for scientists and researchers. In BlueBridge, remote memory pages are identified on the basis of IPv6 addresses. Access and data transfer is connectionless and can easily be transferred across machines. BlueBridge relies on a virtual or SDN switch to route and manages requests.

BlueBridge has several features: 
* A memory daemon which exposes remote memory to clients in the form of a managed virtual address cache or simply as userspace block device.   
* A P4 template implementing a BlueBridge memory protection and cache coherence protocol directly in the switch.
* A specialized RPC framework on the basis of Apache Thrift which uses shared global memory instead of transporting data over TCP streams.
* BlueBridge supports a high-performance linux networking as well as a userspace DPDK stack for fast data transmission.

To build BlueBridge, simply run
`sudo ./install.sh`
and choose the configuration you want to run.

The bluebridge application directory (`ip6/apps/bin`) contains several toy applications and benchmarks:
* **testing**
A simple benchmarking application that tests all functions of the basic BlueBridge API. From the `ip6` directory it can be run as:
`./apps/bin/testing -c CONFIG -i ITERATIONS -t THREADS [-b]`
A sample configuration file can be found in `ip6/distmem_client.cnf`.  `ITERATIONS` specifies the number of roundtrips per API call and the overall size of the memory allocation (beware of exceeding the memory limit of your local machine), THREADS the number of threads to use (default is one), and -b wether to use batch mode (higher throughput) or single block transmissions (low latency).
* **bb_disk**
bb_disk  uses [BUSE](https://github.com/acozzette/BUSE "BUSE") and [NBD](https://github.com/NetworkBlockDevice/nbd "NBD")  to expose BlueBridge as a userspace block device to the operating system. This block device can be formatted as any file system or used as swap device.
* **event_server**
The event server is the simple BlueBridge memory server. It also requires a configuration file, which is given as distmem_server.cnf. The server can be launched using 
 `sudo ./apps/bin/event_server -c [CONFIG]`
* **thrift**
BlueBridge also contains a modified Thrift version which uses shared global BlueBridge memory instead of directly transporting data. 
To run thrift benchmarks run:
`./apps/thrift/tcp_client -c [CONFIG]`
or
`./apps/thrift/ddc_client -c [CONFIG] -i [ITERATIONS]`
Further information can be found in the thrift application and source repository.
* **server (DEPRECATED)**
An old implementation of the BlueBridge server. Instead of using a specialized server networking backend, it uses the generic client send and receive calls. It is no longer maintained.
* **rmem_test (DEPRECATED)**
A toy application to verify the functionality of the BlueBridge virtual memory backend.   This file runs several small test computations which uses a managed mmap space and cache layer. It supports multithreading and several different virtual backends.  The virtual memory system is currently not maintained and not stable.
To run the client page system: `./apps/bin/rmem_test use: {CONFIG] [PAGES] [FRAMES] [MEMORYTYPE] [PROGRAM]"` 
Options for the memory type are: disk, rmem, and mem 
Options for the program are: wc (wordcount) and wc_t (threaded wordcount) 


To run tests, the ip6 repository provides a Mininet emulator. It can be launched using the `sudo python run_emulator.py` command in the `ip6` folder. Mininet  also creates a sample configuration file, which can be found in ip6/tmp/config/distMem.conf after launching the Mininet emulator.
To test thrift, run the emulator with the --thrift-tcp or thrift-udp option. It will start Thrift DDC/TCP servers in addition to the BlueBridge memory servers.
