#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "This script must be run as root"
  exit
fi

# Framework to accept various command line arguments
POSITIONAL=()
while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        -d|--dpdk)
        DPDK=true
        shift # past argument
        shift # past value
        ;;
        *)    # unknown option
        POSITIONAL+=("$1") # save it in an array for later
        shift # past argument
        ;;
    esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters



#git submodule update --init --recursive
sudo apt-get install -y libpcap-dev


if [[ $DPDK ]]; then
    while true;
    do
        read -r -p "Please specify the interface to bind to:" iface
        if [[ $iface ]]; then
            ifconfig $iface > /dev/null 2>&1
            if [[ $? -eq 0 ]]; then
                echo "Initializing the DPDK..."
                break;
            else
                echo "Device not found, please retry..."
            fi
        else
            echo "Not binding to an interface."
            break;
        fi
    done
    git submodule update --init dpdk
    sudo apt-get install -y libnuma-dev
    ##### Optional Installs
    cd dpdk
    make config T=x86_64-native-linuxapp-gcc
    make install T=x86_64-native-linuxapp-gcc
    cd x86_64-native-linuxapp-gcc
    make
    cd ..
    echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
    echo 1024 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
    mkdir /mnt/huge
    mount -t hugetlbfs nodev /mnt/huge
    echo 0 > /proc/sys/kernel/randomize_va_space
    modprobe uio
    insmod x86_64-native-linuxapp-gcc/kmod/igb_uio.ko
    if [[ $iface ]]; then
        ifconfig $iface down
        usertools/dpdk-devbind.py --bind=igb_uio $iface
    fi
    cd ..
else
    git submodule update --init mininet
    git submodule update --init p4
    git submodule update --init nanomsg
    git submodule update --init thrift

    ##### Optional Installs
    sudo apt-get install -y bison
    sudo apt-get install -y flex
    sudo apt-get install -y pkg-config
    sudo apt-get install -y libboost-all-dev
    sudo apt-get install -y libevent-dev
    sudo apt-get install -y libssl-dev
    sudo apt-get install -y libtool
    sudo apt-get install -y libjudy-dev
    sudo apt-get install -y libgmp-dev
    sudo apt-get install -y libffi-dev
    sudo apt-get install -y automake
    sudo apt-get install -y cmake
    sudo apt-get install -y make
    sudo apt-get install -y g++

    # Thrift Folder:
    cd thrift
    ./bootstrap.sh
    ./configure
    make
    make install
    cd ..
    # NanoMsg Folder:
    cd nanomsg
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ctest .
    cmake --build . --target install
    ldconfig
    cd ../..

    # Building the P4 Library:
    cd p4
    ./autogen.sh
    ./configure
    make
    make install
    cd ..
fi


#### NBD Configuration
# sudo apt-get install -y f2fs-tools gparted
# modprobe nbd
#./build/app/busexmp /dev/nbd0
# mkfs.f2fs /dev/nbd0
# mount /dev/nbd0 /mnt

# time sudo rsync -ah --progress file4.txt  test/file4.txt