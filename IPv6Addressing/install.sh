#!/bin/sh
set -x
sudo apt-get install ndppd
./Mininet/util/install.sh -a
make -C messaging/
sudo python ../run-demo.py

