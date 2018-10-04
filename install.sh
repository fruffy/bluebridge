#!/bin/bash
# set -x

read -r -p "Do you want to install the DPDK version? [y/N]: " response
response=${response,,} # tolower
if [[ $response =~ ^(yes|y) ]]; then
    (cd includes && ./setup_includes.sh -d)
    make dpdk
else
    (cd includes && ./setup_includes.sh)
    make
fi

read -r -p "Do you also want to install thrift? [y/N]: " response
response=${response,,} # tolower
if [[ $response =~ ^(yes|y) ]]; then
    sudo apt-get install -y libglib2.0-dev
    cd includes/thrift
    git submodule update --init .
    ./bootstrap.sh
    ./configure --without-cpp
    cd ../..
    make thrift-all
fi

read -r -p "Do you also want to install arrow? [y/N]: " response
response=${response,,} # tolower
if [[ $response =~ ^(yes|y) ]]; then
    sudo apt-get install -y cmake \
         libboost-dev \
         libboost-filesystem-dev \
         libboost-system-dev \
         libbenchmark-dev \
         libgtest-dev
    cd includes/arrow/
    git submodule update --init .
    cd cpp
    mkdir debug
    cd debug
    cmake -DARROW_BUILD_BENCHMARKS=ON -DARROW_BLUEBRIDGE=ON ..
    make unittest
    cd ../..
fi

sudo chown -R $USER:$USER ~

echo "All done! You are good to go!"



