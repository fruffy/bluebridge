#!/bin/sh
set -x
sudo apt-get install ndppd
cp -f make-ndpproxy ndpproxy/Makefile
sudo make install -C ndpproxy/ && make all -C ndpproxy/
#cd mininet
#./util/install.sh -a
#cd -
#make -C messaging/
#sudo python run-demo.py

