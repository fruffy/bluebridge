#!/bin/sh
set -x
git submodule update --init --recursive

# Ubuntu 16.04
# sudo apt-get install -y ndppd

#if [ $? > 0 ]
#then
	# Ubuntu 14.04
	sudo apt-get install -y libnl-3-200
	sudo apt-get install -y libnl-route-3-200
	sudo apt-get install -y libnl-3-dev
	sudo apt-get install -y libnl-route-3-dev
	sudo apt-get install -y libperl-dev
	sudo apt-get install -y libgtk2.0-dev
	cp -f make-ndpproxy ndpproxy/Makefile
	sudo make -C ndpproxy/ install && make -C ndpproxy/ all
#fi

# Ubuntu 16.04
sudo apt-get install -y mininet
if [ $? > 0 ]
then
	# Ubuntu 14.04
	./mininet/util/install.sh -nfv
fi
sudo apt-get install -y libpcap-dev
make -C messaging/
sudo python run-demo.py

