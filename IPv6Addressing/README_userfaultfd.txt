To make: `gcc -Wall -Werror -pthread src/userfaultfd_measure_pagefault.c -o userfaultfd_test -lm`

The program outputs the mean as well as the minimum and maximum latency values. It has the code to calculate the standard deviation and the median, but does not do any outlier detection. 
