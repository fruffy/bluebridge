from math import fsum
import numpy as np
import re
import itertools
import matplotlib as m
import os
import matplotlib.pyplot as plt
import argparse
import math

'''
Helper module for the plot scripts.
'''

import re
import itertools
import matplotlib as m
import os


def read_list(fname, delim=','):
    lines = open(fname).xreadlines()
    ret = []
    for l in lines:
        ls = l.strip().split(delim)
        ls = map(lambda e: '0' if e.strip() == '' or e.strip() == 'ms' or e.strip() == 's' else e, ls)
        ret.append(ls)
    return ret


def ewma(alpha, values):
    if alpha == 0:
        return values
    ret = []
    prev = 0
    for v in values:
        prev = alpha * prev + (1 - alpha) * v
        ret.append(prev)
    return ret


def transpose(l):
    return zip(*l)


def avg(lst):
    return sum(map(float, lst)) / len(lst)


def stdev(lst):
    mean = avg(lst)
    var = avg(map(lambda e: (e - mean)**2, lst))
    return math.sqrt(var)


def cdf(values):
    values.sort()
    prob = 0
    l = len(values)
    x, y = [], []

    for v in values:
        prob += 1.0 / l
        x.append(v)
        y.append(prob)

    return (x, y)


def get_bw(input_file):

    data = read_list("%s/bandwidth.csv" % input_file)

    rate = []
    column = 2
    for row in data:
        try:
            rate.append(float(row[column]) * 8.0 / (1 << 20))
        except:
            break
    avg_rate = avg(rate[10:-10])
    return avg_rate


def get_latencies(input_dir, lat_types, thread):

    lat_results = {}
    for lat_type in lat_types:
        data = read_list("%s/%s_t%d.csv" % (input_dir, lat_type, thread))
        flat_data = [item for subdata in data[1:] for item in subdata]
        lat_results[lat_type] = avg(flat_data) / 1000
    return lat_results


def get_thrift_latencies(input_dir, lat_types, steps_arr):

    lat_results = {}
    for lat_type in lat_types:
        lat_results[lat_type] = []
        for chunk in steps_arr: # range(0, max_size, step_size):
            data = read_list("%s/%s_%d.txt" % (input_dir, lat_type, chunk))
            flat_data = [item for subdata in data[1:] for item in subdata]
            lat_results[lat_type].append(avg(flat_data) / 1000)
    return lat_results


def plot_thread_latency():

    N_THREADS = 65
    it = 1000000
    # lat_types = ["alloc", "write", "read", "free"]
    lat_types = ["write", "read"]

    thread_bw_vals = {}
    thread_lat_vals = {}

    for thread in range(1, N_THREADS):
        dir = "results/testing/t%d_iter%d" % (thread, it)
        bw = get_bw(dir)
        avg_lat = get_latencies(dir, lat_types, thread - 1)
        thread_bw_vals[thread] = bw
        thread_lat_vals[thread] = avg_lat
        print("Thread: %d BW: %f Latency-Read: %f,  Latency-Write: %f" %
              (thread, thread_bw_vals[thread],
               thread_lat_vals[thread]['read'],
               thread_lat_vals[thread]['write']))

    thread_bw_vals[0] = 0
    lists = sorted(thread_bw_vals.items())
    fig1, ax1 = plt.subplots(1)
    thrd, bw = zip(*lists)  # unpack a list of pairs into two tuples
    plt.xlabel('Num of threads')
    plt.ylabel('Throughput (Mbps)')
    plt.xlim(1, N_THREADS - 1)
    plt.xticks([1, 5, 8, 10, 20, 30, 40, 50, 60, 64, N_THREADS - 1])
    xcoords = [8, 16, 24, 32, 40, 48, 56, 64]
    for xc in xcoords:
        plt.axvline(x=xc, linestyle=':', color='#C0C0C0')
    ax1.plot(thrd, bw, linestyle='--', color='r', label='BW')
    plt.savefig("graph_gen_code/microbench_bandwidth.png")

    lists = sorted(thread_lat_vals.items())
    fig2, ax2 = plt.subplots(1)
    thrd, lats = zip(*lists)  # unpack a list of pairs into two tuples
    read_lats = [d['read'] for d in lats]
    write_lats = [d['write'] for d in lats]

    plt.xlabel('Num of threads')
    plt.ylabel('Latency (us)')
    plt.xlim(1, N_THREADS - 1)
    plt.xticks([1, 5, 8, 10, 20, 30, 40, 50, 60, 64, N_THREADS - 1])
    xcoords = [8, 16, 24, 32, 40, 48, 56, 64]
    for xc in xcoords:
        plt.axvline(x=xc, linestyle=':', color='#C0C0C0')
    ax2.plot(thrd, read_lats, linestyle='--', color='g', label='Read')
    ax2.plot(thrd, write_lats, linestyle='--', color='r', label='Write')
    plt.legend(loc='upper left')
    plt.savefig("graph_gen_code/microbench_latency.png")


def plot_thrift_latencies():
    lat_types = ["incr_arr_rpc_lat", "add_arr_rpc_lat"]
    tests = ["ddc", "tcp"]
    # max_size = 4096 * 8 # 8192000
    # step_size = 100
    NUM_STEPS = 10000/10
    # steps_arr = list((2**exp for exp in range(NUM_STEPS)))
    steps_arr = list(range(0,10000,10))
    dir = "../results/thrift/"
    lats = {}
    for lat_type in lat_types:
        x_step = steps_arr # list(range(0, max_size, step_size))
        for test in tests:
            lats = get_thrift_latencies(dir + test, lat_types, steps_arr)
            plt.plot(x_step, lats[lat_type], label="Latency %s" % test)
        plt.xlabel('Array Size')
        plt.ylabel('Latency')
        plt.legend(loc='upper left')
        plt.savefig("%s_%d" % (lat_type, steps_arr[NUM_STEPS-1]))
        plt.gcf().clear()


if __name__ == '__main__':
    # plot_thread_latency()
    plot_thrift_latencies()
