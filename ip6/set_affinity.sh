#!/bin/bash
(let CPU=0; cd /sys/class/net/enp66s0f0/device/msi_irqs/;  
         for IRQ in *; do
            echo $CPU > /proc/irq/$IRQ/smp_affinity_list
            let CPU+=1
         done)
