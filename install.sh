#!/bin/bash
# set -x

cd includes
read -r -p "Do you want to install the DPDK version? [y/N]: " response
response=${response,,} # tolower
if [[ $response =~ ^(yes|y) ]]; then
    ./setup_includes.sh -d
    cd ..
    make dpdk
else 
    ./setup_includes.sh
    cd ..
    make
fi
sudo chown -R $USER:$USER .
echo "All done! You are good to go!"



