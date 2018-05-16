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
    sudo apt-get install libglib2.0-dev
    cd includes/thrift
    git submodule update --init .
    ./bootstrap.sh
    ./configure --without-cpp
    cd ../..
    make thrift-all
fi

sudo chown -R $USER:$USER ~

echo "All done! You are good to go!"



