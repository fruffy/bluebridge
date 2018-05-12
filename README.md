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

To run tests, the ip6 repository provides a Mininet emulator. It can be launched using the `sudo python run_emulator.py` command. BlueBridge can then be benchmarked  using `./apps/bin/testing -c [CONFIG] -i [ITERATIONS] -t [THREADS] `. A sample configuration file can be found in ip6/tmp/config/distMem.conf after launching the Mininet emulator.
To test thrift, run the emulator with the --thrift-tcp or thrift-udp option. It will start Thrift DDC/TCP servers in addition to the BlueBridge memory servers. To run thrift benchmarks run `./apps/thrift/tcp_client -c [CONFIG]` or `./apps/thrift/ddc_client -c [CONFIG] -i [ITERATIONS]`.
