#!/bin/bash
# set -x

cd includes
read -r -p "Do you want to install the DPDK version? [y/N]: " response
response=${response,,} # tolower
if [[ $response =~ ^(yes|y) ]]; then
    sudo ./setup_includes.sh -d
    make dpdk
else 
    sudo ./setup_includes.sh
    make
fi
cd ..

echo "All done! You are good to go!"



