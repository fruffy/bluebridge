# Install and Run Instructions

After pulling down the latest master, run `install.sh` from the IPv6Addressing folder. This will install the following packages, make the code, and run `run-demo.py`.
  - mininet
  - libpcap-dev
  
# Compile Instructions
If you wish to compile the program manually, run `make -C messaging/` from the BlueBridge directory.

# Run Instructions
Common commands are: 

## Demo
To run the demo call `sudo python run-demo.py`.

## Client and Server
To run the server: `sudo ./application/bin/server [CONFIG]`
To run the client test: `./application/bin/testing -c [CONFIG] -i [ITERATIONS] -t [THREADS] `
To run the client page system: `./messaging/bin/rmem_test use: {CONFIG] [PAGES] [FRAMES] [MEMORYTYPE] [PROGRAM]"`
Options for the memory type are: disk, rmem, and mem
Options for the program are: wc (wordcount) and wc_t (threaded wordcount)

