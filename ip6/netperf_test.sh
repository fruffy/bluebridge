#! /bin/bash

COUNTER=0
OPERATION="UDP"
BUF_SIZE=2097153

while [ $COUNTER -lt 22 ]; do
	SIZE=$((2**$COUNTER))
	echo "Size: $SIZE, buf size: $BUF_SIZE"

	if [ $OPERATION = "TCP" ]; then
		echo "netperf -t TCP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -s $BUF_SIZE,$BUF_SIZE -S $BUF_SIZE,$BUF_SIZE" >> netperf_tcp_rr.txt
		netperf -t TCP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -s $BUF_SIZE,$BUF_SIZE -S $BUF_SIZE,$BUF_SIZE >> netperf_tcp_rr.txt
	else
		echo "netperf -t UDP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -s $BUF_SIZE,$BUF_SIZE -S $BUF_SIZE,$BUF_SIZE" >> netperf_udp_rr.txt
		netperf -t UDP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -s $BUF_SIZE,$BUF_SIZE -S $BUF_SIZE,$BUF_SIZE >> netperf_udp_rr.txt
	fi
	let COUNTER=COUNTER+1
done