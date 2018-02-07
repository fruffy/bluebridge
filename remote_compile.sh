#!/bin/bash
set +x
USER_R="fruffy"
DIR_LOCAL="/home/fruffy/Projects/BlueBridge"
DIR_REMOTE="/home/$USER_R/ip6/BlueBridge"
SERVER1="192.168.79.28"
SERVER2="192.168.79.41"
SERVER3="192.168.79.27"

IP6TARGET="$DIR_LOCAL/ip6/lib "
IP6TARGET+="$DIR_LOCAL/ip6/applications/ "
IP6TARGET+="$DIR_LOCAL/ip6/*.mk "

BBTARGET="makefile"

scp -r $IP6TARGET $USER_R@$SERVER1:$DIR_REMOTE/ip6/
scp -r $BBTARGET $USER_R@$SERVER1:$DIR_REMOTE/
ssh $USER_R@$SERVER1 make -C $DIR_REMOTE dpdk

scp -r $IP6TARGET $USER_R@$SERVER2:$DIR_REMOTE/ip6/
ssh  $USER_R@$SERVER2 make -C $DIR_REMOTE dpdk
scp -r $BBTARGET $USER_R@$SERVER2:$DIR_REMOTE/

# scp -r $DIR_LOCAL/ip6/lib $USER@$SERVER3:$DIR_REMOTE/ip6/
# scp -r $DIR_LOCALip6/applications/ $USER@$SERVER3:$DIR_REMOTE/ip6/
# ssh    $USER@$SERVER3 make -C $DIR_REMOTE 

