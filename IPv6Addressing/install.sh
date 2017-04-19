#!/bin/sh
set -x

sudo apt-get install ndppd

if [[ $? > 0 ]]
then
	sudo apt-get install libnl-3-dev
	sudo apt-get install libnl-route-3-dev
	cp -f make-ndpproxy ndpproxy/Makefile
	sudo make -C ndpproxy/ install && make -C ndpproxy/ all
fi

sudo apt-get install mininet
if [[ $? > 0 ]]
then
	./mininet/util/install.sh -a
fi

make -C messaging/
sudo python run-demo.py

