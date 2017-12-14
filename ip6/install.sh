#!/bin/sh
set -x
git submodule update --init --recursive
./mininet/util/install.sh -nfv
sudo apt-get install -y libpcap-dev
make -C ../

