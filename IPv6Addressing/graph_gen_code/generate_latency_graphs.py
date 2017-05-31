import os
import sys
import re
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import matplotlib
import math
from math import sqrt
from operator import add
from matplotlib.ticker import FormatStrFormatter
import matplotlib.colors as colors
SPINE_COLOR = 'gray'

all_data = [None, None, None, None, None, None]
microbench = None

class MicroBenchData:
    def __init__(self, alloc, read, write, free, conversion=1000.0):
        self.alloc_latency = alloc #reject_outliers(np.array(alloc), 3)
        self.read_latency = read #reject_outliers(np.array(read), 3) 
        self.write_latency = write #reject_outliers(np.array(write), 3)
        self.free_latency = free #reject_outliers(np.array(free), 3) 
        self.conv = conversion

    def getMedian(self):
        alloc_med = np.median(self.alloc_latency)/self.conv
        read_med = np.median(self.read_latency)/self.conv
        write_med = np.median(self.write_latency)/self.conv
        free_med = np.median(self.free_latency)/self.conv

        return alloc_med, read_med, write_med, free_med

    def getPercentile(self, percentile):
        alloc = np.percentile(self.alloc_latency, percentile)/self.conv
        read = np.percentile(self.read_latency, percentile)/self.conv
        write = np.percentile(self.write_latency, percentile)/self.conv
        free = np.percentile(self.free_latency, percentile)/self.conv

        return alloc, read, write, free

class Data:
    def __init__(self, dtype, tl, enf, exf, h, r1, gd, conversion=1000.0):
        self.dtype = dtype
        self.total_latency = tl # reject_outliers(np.array(tl), .1)
        self.enter_fault = enf # reject_outliers(np.array(enf), .1)
        self.exit_fault = exf # reject_outliers(np.array(exf), .1)
        self.handler_latency = h # reject_outliers(np.array(h), .1)
        self.conv = conversion

        if dtype == 'local':
            self.get_data = gd # reject_outliers(np.array(gd), 1)
        else:
            if np.count_nonzero(r1) > 0:
                self.rtt1 = r1 # reject_outliers(np.array(r1), .1)
            else:
                self.rtt1 = np.array(r1)
            self.get_data = gd # reject_outliers(np.array(gd), .1)

    def getMedian(self):
        if self.dtype == 'local':
            total_latency_med = np.median(self.total_latency)/self.conv
            get_data_med = np.median(self.get_data)/self.conv
            enter_fault_med = np.median(self.enter_fault)/self.conv
            exit_fault_med = np.median(self.exit_fault)/self.conv

            everything = get_data_med+enter_fault_med+exit_fault_med
        
            return (total_latency_med, 0, get_data_med, enter_fault_med, exit_fault_med, everything)
        else:
            total_latency_med = np.median(self.total_latency)/self.conv
            rtt1_med = np.median(self.rtt1)/self.conv
            get_data_med = np.median(self.get_data)/self.conv
            enter_fault_med = np.median(self.enter_fault)/self.conv
            exit_fault_med = np.median(self.exit_fault)/self.conv

            everything = rtt1_med+get_data_med+enter_fault_med+exit_fault_med

            return (total_latency_med, rtt1_med, get_data_med, enter_fault_med, exit_fault_med, everything)

    def getPercentile(self, percentile):
        if self.dtype == 'local':
            total = np.percentile(self.total_latency, percentile)/self.conv
            get_data = np.percentile(self.get_data, percentile)/self.conv
            enter_fault = np.percentile(self.enter_fault, percentile)/self.conv
            exit_fault = np.percentile(self.exit_fault, percentile)/self.conv

            return (total, 0, get_data, enter_fault, exit_fault)
        else:
            total = np.percentile(self.total_latency, percentile)/self.conv
            rtt1 = np.percentile(self.rtt1, percentile)/self.conv
            get_data = np.percentile(self.get_data, percentile)/self.conv
            enter_fault = np.percentile(self.enter_fault, percentile)/self.conv
            exit_fault = np.percentile(self.exit_fault, percentile)/self.conv

            return (total, rtt1, get_data, enter_fault, exit_fault)

def getDataIndex(dtype, access_type, ds):
    if dtype == 'local':
        if access_type == 'read':
            return 2
        else:
            return 5
    else:
        if access_type == 'read':
            if ds == 'ds':
                return 0
            else:
                return 1
        else:
            if ds == 'ds':
                return 3
            else:
                return 4

def reject_outliers(data, m=2):
    return data[abs(data - np.mean(data)) < m * np.std(data)]

def latexify(fig_width=None, fig_height=None, columns=1):
        """Set up matplotlib's RC params for LaTeX plotting.
        Call this before plotting a figure.

        Parameters
        ----------
        fig_width : float, optional, inches
        fig_height : float,  optional, inches
        columns : {1, 2}
        """

        # code adapted from http://www.scipy.org/Cookbook/Matplotlib/LaTeX_Examples

        # Width and max height in inches for IEEE journals taken from
        # computer.org/cms/Computer.org/Journal%20templates/transactions_art_guide.pdf

        assert(columns in [1,2])

        if fig_width is None:
                fig_width = 3.39 if columns==1 else 6.9 # width in inches

        if fig_height is None:
                golden_mean = (sqrt(5)-1.0)/2.0    # Aesthetic ratio
                fig_height = fig_width*golden_mean # height in inches

        MAX_HEIGHT_INCHES = 8.0
        if fig_height > MAX_HEIGHT_INCHES:
                print("WARNING: fig_height too large:" + fig_height + 
                            "so will reduce to" + MAX_HEIGHT_INCHES + "inches.")
                fig_height = MAX_HEIGHT_INCHES

        params = {'backend': 'ps',
                            'text.latex.preamble': ['\usepackage{gensymb}'],
                            'axes.labelsize': 10, # fontsize for x and y labels (was 10)
                            'axes.titlesize': 10,
                            'font.size': 10, # was 10
                            'legend.fontsize': 7, # was 10
                            'xtick.labelsize': 8,
                            'ytick.labelsize': 8,
                            'text.usetex': True,
                            'figure.figsize': [fig_width,fig_height],
                            'font.family': 'serif'
        }

        matplotlib.rcParams.update(params)

def format_axes(ax, bottom_tick=True):
        for spine in ['top', 'right']:
                ax.spines[spine].set_visible(False)

        for spine in ['left', 'bottom']:
                ax.spines[spine].set_color(SPINE_COLOR)
                ax.spines[spine].set_linewidth(0.5)

        if bottom_tick:
            ax.xaxis.set_ticks_position('bottom')
        ax.yaxis.set_ticks_position('left')

        for axis in [ax.xaxis, ax.yaxis]:
            axis.set_tick_params(direction='out', color=SPINE_COLOR)

        return ax

def usage():
    print("USAGE: python generate_latency_graphs.py <data folder>")

def parse_input_file(input_file, raw_data):
    # print("Parsing " + os.path.basename(input_file))

    ftype = os.path.basename(input_file).split('_')[0]

    first = True
    for line in open(input_file):
        if first:
            first = False
        else:
            parse_line(line, ftype, raw_data)

def parse_line(line, ftype, raw_data):
    if ftype == "first":
        raw_data[0].append((int(line.split(',')[0]), int(line.split(',')[1])))
    elif ftype == "second":
        raw_data[1].append((int(line.split(',')[0]), int(line.split(',')[1])))
    elif ftype == "total":
        raw_data[2].append((int(line.split(',')[0]), int(line.split(',')[1])))
    elif ftype == "handler":
        raw_data[3].append((int(line.split(',')[0]), int(line.split(',')[1])))
    else:
        print("ERROR: unknown type ", ftype)
        sys.exit()

def parse_input_folder(input_dir):
    global all_data
    total_latency = []
    rtt1 = []
    rtt2 = []
    enter_fault = []
    exit_fault = []
    handler_latency = []

    first_rtt = []
    second_rtt = []
    total = []
    handler = []

    raw_data = (first_rtt, second_rtt, total, handler)

    remote = False
    if "remote" in input_dir:
        remote = True
    input_dir = os.path.abspath(input_dir)

    if not os.path.isdir(input_dir):
        print("ERROR: takes in input directory of latency files")
        sys.exit()

    for input_file in os.listdir(input_dir):
        if input_file.endswith(".csv"):
            parse_input_file(os.path.join(input_dir, input_file), raw_data)

    if not raw_data[0] or not raw_data[1] or not raw_data[2] or not raw_data[3]:
        print("ERROR: parsing did not work")
        print("RTT1: ", first_rtt)
        print("RTT2: ", second_rtt)
        print("Total: ", total)
        print("Handler: ", handler)
        sys.exit()


    # Get breakdown categories from data
    for i in range(len(total)):
        total_latency.append(total[i][1] - total[i][0])
        rtt1.append(first_rtt[i][1] - first_rtt[i][0])
        rtt2.append(second_rtt[i][1] - second_rtt[i][0])
        enter_fault.append(total[i][0] - handler[i][0]) 
        exit_fault.append(total[i][1] - handler[i][1])
        handler_latency.append(handler[i][1] - handler[i][0])

    folder_name = os.path.basename(input_dir)
    dtype, access_type, ds = parse_folder_name(folder_name)

    print("Adding data for " + folder_name)

    all_data[getDataIndex(dtype, access_type, ds)] = Data(dtype, total_latency, enter_fault, exit_fault, handler_latency, rtt1, rtt2)

def parse_folder_name(folder_name):
    splitname = folder_name.split('_')

    if len(splitname) == 2:
        splitname.append("noop")

    return splitname

def parse_microbench(folder_name):
    global microbench
    # Alloc, read, write, free
    latencies = [[], [], [], []]

    folder_name = os.path.abspath(folder_name)

    for input_file in os.listdir(folder_name):
        # print(input_file)
        if input_file.endswith(".csv"):
            latencies[getMicroIndex(input_file)] = parse_micro_file(os.path.join(folder_name, input_file))

    # Add to microbench data
    microbench = MicroBenchData(latencies[0], latencies[1], latencies[2], latencies[3])

def parse_micro_file(file_name):
    with open(file_name) as f:
        temp = f.readlines()[1:] # Ignore header
        temp = [int(x.strip()) for x in temp]
    return temp

def getMicroIndex(name):
    if "alloc" in name:
        return 0
    elif "read" in name:
        return 1
    elif "write" in name:
        return 2
    elif "free" in name:
        return 3

def get_micro_errors():
    errors = []
    top = microbench.getPercentile(95)
    bottom = microbench.getPercentile(5)
    median = microbench.getMedian()

    for i in range(len(top)):
        errors.append([abs(median[i]-bottom[i]), abs(median[i] - top[i])])

    return errors

def display_microbench():
    fig, ax = plt.subplots()

    width = 0.25
    ind = np.arange(4)

    errors = get_micro_errors()

    ax.bar(ind, microbench.getMedian(), color='White', yerr=np.array(errors).T, error_kw={'ecolor':'Black', 'linewidth':2})

    ax.set_xticks(ind+1.5*width)
    ax.set_xticklabels(['Alloc', 'Read', 'Write', 'Free'])

    ax.set_ylabel("Latency ($\micro$s)")

    plt.tight_layout()
    format_axes(ax)
    plt.savefig("Microbenchmarks_Latency.pdf")

def display_breakdown(btype, values):
    # Displays the breakdown of latencies for each operation: read, write
    fig, ax = plt.subplots()

    width = 0.15
    patterns = ('---', 'xxx', '+++', '///', 'ooo', 'OOO', '...')
    ind = np.arange(3)

    enterf, exitf, access, getdata, other, enterf_err, exitf_err, access_err, getdata_err = values


    ax.bar(ind, enterf, width, color='White', hatch=patterns[0], label='Enter handler', yerr=np.array(enterf_err).T, error_kw={'ecolor':'Black', 'linewidth':2})
    ax.bar(ind+width, exitf, width, color='White', hatch=patterns[1], label='Exit handler', yerr=np.array(exitf_err).T, error_kw={'ecolor':'Black', 'linewidth':2})
    ax.bar(ind+2*width, access, width, color='White', hatch=patterns[2], label='Access DS', yerr=np.array(access_err).T, error_kw={'ecolor':'Black', 'linewidth':2})
    ax.bar(ind+3*width, getdata, width, color='White', hatch=patterns[3], label='Get data', yerr=np.array(getdata_err).T, error_kw={'ecolor':'Black', 'linewidth':2})
    ax.bar(ind+4*width, other, width, color='White', hatch=patterns[4], label='Other')

    ax.set_xticks(ind+2.5*width)
    ax.set_xticklabels(['Remote\_DS', 'Remote\_NoDS', 'Local'])

    ax.set_ylabel("Latency ($\micro$s)")
    lgd = ax.legend(loc='upper right')
    plt.tight_layout()
    format_axes(ax)
    plt.savefig("Latency_Breakdown_" + btype + ".pdf", bbox_extra_artists=(lgd,), bbox_inches='tight')

def getDataError(data):
    errors = []
    top = data.getPercentile(95)
    bottom = data.getPercentile(5)
    median = data.getMedian()

    for i in range(len(top)):
        errors.append([abs(median[i]-bottom[i]), abs(median[i] - top[i])])


    return median, errors

def generate_breakdown(btype):
    global all_data
    #total_latency_med, rtt1_med, rtt2_med, enter_fault_med, exit_fault_med, everything
    if btype == 'read':
        remote_ds, remote_ds_err = getDataError(all_data[0])
        remote_nods, remote_nods_err = getDataError(all_data[1])
        local, local_err = getDataError(all_data[2])
    else:
        remote_ds, remote_ds_err = getDataError(all_data[3])
        remote_nods, remote_nods_err = getDataError(all_data[4])
        local, local_err = getDataError(all_data[5])

    enterf = [remote_ds[3], remote_nods[3], local[3]]
    enterf_err = [remote_ds_err[3], remote_nods_err[3], local_err[3]]
    
    exitf = [remote_ds[4], remote_nods[4], local[4]]
    exitf_err = [remote_ds_err[4], remote_nods_err[4], local_err[4]]

    access = [remote_ds[1], remote_nods[1], local[1]]
    access_err = [remote_ds_err[1], remote_nods_err[1], local_err[1]]

    getdata = [remote_ds[2], remote_nods[2], local[2]]
    getdata_err = [remote_ds_err[2], remote_nods_err[2], local_err[2]]

    other = [remote_ds[0] - remote_ds[5], remote_nods[0] - remote_nods[5], local[0] - local[5]]
    # other_err = [remote_ds[0] - remote_ds[5], remote_nods[0] - remote_nods[5], local[0] - local[5]]

    return enterf, exitf, access, getdata, other, enterf_err, exitf_err, access_err, getdata_err, #other_err

def display_total():
    fig, ax = plt.subplots()

    width = 0.25
    patterns = ('---', 'xxx', '+++', '///', 'ooo', 'OOO', '...')
    ind = np.arange(2)

    remote_nods, remote_ds, local, remote_nods_err, remote_ds_err, local_err = get_latencies()

    ax.bar(ind, remote_ds, width, color='White', hatch=patterns[0], label='Remote w/ DS', yerr=np.array(remote_ds_err).T, error_kw={'ecolor':'Black', 'linewidth':2})
    ax.bar(ind+width, remote_nods, width, color='White', hatch=patterns[1], label='Remote w/o DS', yerr=np.array(remote_nods_err).T, error_kw={'ecolor':'Black', 'linewidth':2})
    ax.bar(ind+2*width, local, width, color='White', hatch=patterns[2], label='Local', yerr=np.array(local_err).T, error_kw={'ecolor':'Black', 'linewidth':2})

    ax.set_xticks(ind+1.5*width)
    ax.set_xticklabels(['Read', 'Write'])

    ax.set_ylabel("Latency ($\micro$s)")
    lgd = ax.legend(bbox_to_anchor=(0.135, 0.99), loc='upper left')
    # plt.tight_layout()
    format_axes(ax)
    plt.savefig("Latency_Total_Comparison.pdf", bbox_extra_artists=(lgd,), bbox_inches='tight')

def get_latencies():
    global all_data
    # import pdb; pdb.set_trace()
    rr_ds_med, rr_ds_err = getDataError(all_data[0])
    rr_nods_med, rr_nods_err = getDataError(all_data[1])
    lr_med, lr_err = getDataError(all_data[2])
    rw_ds_med, rw_ds_err = getDataError(all_data[3])
    rw_nods_med, rw_nods_err = getDataError(all_data[4])
    lw_med, lw_err = getDataError(all_data[5])

    remote_ds = [rr_ds_med[0], rw_ds_med[0]]
    remote_ds_err = [rr_ds_err[0], rw_ds_err[0]]

    remote_nods = [rr_nods_med[0], rw_nods_med[0]]
    remote_nods_err = [rr_nods_err[0], rw_nods_err[0]]

    local = [lr_med[0], lw_med[0]]
    local_err = [lr_err[0], lw_err[0]]

    return remote_nods, remote_ds, local, remote_nods_err, remote_ds_err, local_err


if __name__ == '__main__':
    input_dir = sys.argv[1]
    latexify()

    if not os.path.isdir(input_dir):
        print("ERROR: takes in input directory of latency files")
        sys.exit()

    for sub_folder in os.listdir(input_dir):
        # print("Looking at %s" % sub_folder)
        full_path = os.path.join(input_dir, sub_folder)
        if os.path.isdir(full_path):
            if "microbench" in sub_folder:
                # print("Parsing microbenchmarks")
                parse_microbench(full_path)
            else:   
                parse_input_folder(full_path)
        else:
            print("Could not parse %s" % sub_folder)

    display_microbench()

    display_total()

    display_breakdown("Read", generate_breakdown("read"))
    display_breakdown("Write", generate_breakdown("write"))