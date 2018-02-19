#!/bin/bash


#### NBD Configuration
# sudo apt-get install -y f2fs-tools gparted
# modprobe nbd
# ./build/app/busexmp /dev/nbd0
# mkfs.f2fs /dev/nbd0
# mount /dev/nbd0 /mnt

# time sudo rsync -ah --progress file4.txt  test/file4.txt