#!/bin/bash

DPDK_F=../includes/dpdk


if [ "$EUID" -ne 0 ]
  then echo "This script must be run as root"
  exit
fi
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
echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
echo 1024 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
mkdir /mnt/huge
mount -t hugetlbfs nodev /mnt/huge
echo 0 > /proc/sys/kernel/randomize_va_space
modprobe uio
insmod $DPDK_F/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko
if [[ $iface ]]; then
    ifconfig $iface down
    $DPDK_F/usertools/dpdk-devbind.py --bind=igb_uio $iface
fi
