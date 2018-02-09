#!/bin/bash

    while true;
    do
        read -r -p "Please specify the interface to bind to:" iface
        ifconfig $iface > /dev/null 2>&1
        if [[ $? -eq 0 ]]; then
            echo "Initializing the DPDK..."
            break;
        else
            echo "Device not found, please retry..."
        fi
    done