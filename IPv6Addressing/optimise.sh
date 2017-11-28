#!/bin/bash
ID=`id -u`
if [ $ID -ne 0 ]; then
   echo "This command must be run as root."
   exit 1
fi
ethtool -N enp66s0f0 rx-flow-hash udp6 sdfn
ethtool -C enp66s0f0 rx-usecs 0
#sudo ethtool --offload  enp66s0f0  rx on tx on
ethtool -A enp66s0f0 autoneg off rx off tx off
ip6tables -t raw -I PREROUTING 1 --src 0:0:100::/40 -j NOTRACK
ip6tables -I INPUT 1 --src 0:0:100::/40 -j ACCEPT

ncpus=`grep -ciw ^processor /proc/cpuinfo`
test "$ncpus" -gt 1 || exit 1
n=0
for irq in `cat /proc/interrupts | grep eth | awk '{print $1}' | sed s/\://g`
do
    f="/proc/irq/$irq/smp_affinity"
    test -r "$f" || continue
    cpu=$[$ncpus - ($n % $ncpus) - 1]
    if [ $cpu -ge 0 ]
            then
                mask=`printf %x $[2 ** $cpu]`
                echo "Assign SMP affinity: eth queue $n, irq $irq, cpu $cpu, mask 0x$mask"
                echo "$mask" > "$f"
                let n+=1
    fi
done
