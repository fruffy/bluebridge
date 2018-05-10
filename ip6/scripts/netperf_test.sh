#! /bin/bash

COUNTER=0

while [ $COUNTER -lt 22 ]; do
	SIZE=$((2**$COUNTER))
	echo "Size: $SIZE"

	echo "netperf -t TCP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -o P50_LATENCY,P90_LATENCY,P99_LATENCY,MEAN_LATENCY,STDDEV_LATENCY" >> netperf_tcp_rr.txt
	netperf -t TCP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -o P50_LATENCY,P90_LATENCY,P99_LATENCY,MEAN_LATENCY,STDDEV_LATENCY >> netperf_tcp_rr.txt
	echo "netperf -t UDP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -o P50_LATENCY,P90_LATENCY,P99_LATENCY,MEAN_LATENCY,STDDEV_LATENCY" >> netperf_udp_rr.txt
	netperf -t UDP_RR -H 0:0:102:: -v 2 -- -r $SIZE,$SIZE -o P50_LATENCY,P90_LATENCY,P99_LATENCY,MEAN_LATENCY,STDDEV_LATENCY >> netperf_udp_rr.txt
	let COUNTER=COUNTER+1
done