#!/bin/bash

# if [ "$EUID" -ne 0 ]
#   then echo "This script must be run as root"
#   exit
# fi

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
    sudo apt install python-pip
    git submodule update --init mininet
    sudo mininet/util/install.sh -nfv
    git submodule update --init p4
    sudo -H pip install ipaddr
    ##### Optional Installs
    (cd p4 && ./install_deps.sh)
    echo "Installing P4"
    (cd p4/ && ./autogen.sh)
    (cd p4/ && ./configure --with-pdfixed)
    (cd p4/ && make)
    (cd p4/ && sudo make install)
    echo "Installing P4"
    (cd p4/ && ./autogen.sh)
    (cd p4/ && ./configure --with-pdfixed)
    (cd p4/ && make)
    (cd p4/ && sudo make install)
fi
