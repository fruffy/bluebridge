#!/bin/sh
set -x
./Mininet/util/install.sh -a
cd messaging
make
cd -

