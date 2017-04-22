#Install and Run Instructions

After pulling down the latest master, run `install.sh` from the IPv6Addressing folder. This will install the following packages, make the code, and run `run-demo.py`.
  - libnl-3-200
  - libnl-route-3-200
  - libnl-3-dev
  - libnl-route-3-dev
  - libperl-dev
  - libgtk2.0-dev
  - mininet
  - libpcap-dev
  
#Compile Instructions
If you wish to compile the program manually, run `make -C messaging/` from the IPv6Addressing directory.

#Run Instructions
##Demo
To run the demo call `sudo python run-demo.py`.
##Client and Server
To run the client: `./messaging/bin/client -i`
To run the server: `./messaging/bin/server`

The `-i` option runs the program in interactive mode. 

##Userfaultfd and Server
To run the userfaultfd program: `./messaging/bin/userfaultfd_measure_pagefault`
Run the server as specified above.
