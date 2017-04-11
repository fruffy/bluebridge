#!/bin/sh
set -x
sudo apt-get install ndppd
./mininet/util/install.sh -a
make -C messaging/
sudo python run-demo.py

