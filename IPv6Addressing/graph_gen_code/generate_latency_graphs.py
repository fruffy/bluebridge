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

class Data:
    def __init__(self, dtype, tl, enf, exf, h, r1, gd, conversion=1000.0):
        self.dtype = dtype
        self.total_latency = tl
        self.enter_fault = enf
        self.exit_fault = exf
        self.handler_latency = h
        self.conv = conversion

        if dtype == 'local':
            self.get_data = gd
        else:
            self.rtt1 = r1
            self.rtt2 = gd

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
            rtt2_med = np.median(self.rtt2)/self.conv
            enter_fault_med = np.median(self.enter_fault)/self.conv
            exit_fault_med = np.median(self.exit_fault)/self.conv

            everything = rtt1_med+rtt2_med+enter_fault_med+exit_fault_med

            return (total_latency_med, rtt1_med, rtt2_med, enter_fault_med, exit_fault_med, everything)

    def get95th(self):
        if self.dtype == 'local':
            total_95 = np.percentile(self.total_latency, 95)/self.conv
            get_data_95 = np.percentile(self.get_data, 95)/self.conv
            enter_fault_95 = np.percentile(self.enter_fault, 95)/self.conv
            exit_fault_95 = np.percentile(self.exit_fault, 95)/self.conv

            return (total_95, 0, get_data_95, enter_fault_95, exit_fault_95)
        else:
            total_95 = np.percentile(self.total_latency, 95)/self.conv
            rtt1_95 = np.percentile(self.rtt1, 95)/self.conv
            rtt2_95 = np.percentile(self.rtt2, 95)/self.conv
            enter_fault_95 = np.percentile(self.enter_fault, 95)/self.conv
            exit_fault_95 = np.percentile(self.exit_fault, 95)/self.conv

            return (total_95, rtt1_95, rtt2_95, enter_fault_95, exit_fault_95)

    def get5th(self):
        if self.dtype == 'local':
            total_5 = np.percentile(self.total_latency, 5)/self.conv
            get_data_5 = np.percentile(self.get_data, 5)/self.conv
            enter_fault_5 = np.percentile(self.enter_fault, 5)/self.conv
            exit_fault_5 = np.percentile(self.exit_fault, 5)/self.conv

            return (total_5, 0, get_data_5, enter_fault_5, exit_fault_5)
        else:
            total_5 = np.percentile(self.total_latency, 5)/self.conv
            rtt1_5 = np.percentile(self.rtt1, 5)/self.conv
            rtt2_5 = np.percentile(self.rtt2, 5)/self.conv
            enter_fault_5 = np.percentile(self.enter_fault, 5)/self.conv
            exit_fault_5 = np.percentile(self.exit_fault, 5)/self.conv

            return (total_5, rtt1_5, rtt2_5, enter_fault_5, exit_fault_5)

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
    print("Parsing " + os.path.basename(input_file))

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

    # import pdb; pdb.set_trace()

    # Get breakdown categories from data
    for i in range(len(total)):
        total_latency.append(total[i][1] - total[i][0])
        rtt1.append(first_rtt[i][1] - first_rtt[i][0])
        rtt2.append(second_rtt[i][1] - second_rtt[i][0])
        enter_fault.append(total[i][0] - handler[i][0]) 
        exit_fault.append(total[i][1] - handler[i][1])
        handler_latency.append(handler[i][1] - handler[i][0])

    # import pdb; pdb.set_trace()
    # TODO: figure out what graph it is
    folder_name = os.path.basename(input_dir)
    dtype, access_type, ds = parse_folder_name(folder_name)

    all_data[getDataIndex(dtype, access_type, ds)] = Data(dtype, total_latency, enter_fault, exit_fault, handler_latency, rtt1, rtt2)

def parse_folder_name(folder_name):
    splitname = folder_name.split('_')

    if len(splitname) == 2:
        splitname.append("noop")

    return splitname


def display_breakdown(btype, values):
    # Displays the breakdown of latencies for each operation: read, write
    fig, ax = plt.subplots()

    width = 0.15
    patterns = ('---', 'xxx', '+++', '///', 'ooo', 'OOO', '...')
    ind = np.arange(3)

    enterf, exitf, access, getdata, other = values

    ax.bar(ind, enterf, width, color='White', hatch=patterns[0], label='Enter handler')
    ax.bar(ind+width, exitf, width, color='White', hatch=patterns[1], label='Exit handler')
    ax.bar(ind+2*width, access, width, color='White', hatch=patterns[2], label='Access DS')
    ax.bar(ind+3*width, getdata, width, color='White', hatch=patterns[3], label='Get data')
    ax.bar(ind+4*width, other, width, color='White', hatch=patterns[4], label='Other')

    ax.set_xticks(ind+2.5*width)
    ax.set_xticklabels(['Remote\_DS', 'Remote\_NoDS', 'Local'])

    ax.set_ylabel("Latency ($\micro$s)")
    lgd = ax.legend(loc='upper right')
    plt.tight_layout()
    format_axes(ax)
    plt.savefig("Latency_Breakdown_" + btype + ".pdf", bbox_extra_artists=(lgd,), bbox_inches='tight')

def generate_breakdown(btype):
    #total_latency_med, rtt1_med, rtt2_med, enter_fault_med, exit_fault_med, everything
    if btype == 'read':
        remote_ds = all_data[0].getMedian()
        remote_nods = all_data[1].getMedian()
        local = all_data[2].getMedian()
    else:
        remote_ds = all_data[3].getMedian()
        remote_nods = all_data[4].getMedian()
        local = all_data[5].getMedian()

    enterf = [remote_ds[3], remote_nods[3], local[3]]
    exitf = [remote_ds[4], remote_nods[4], local[4]]
    access = [remote_ds[1], remote_nods[1], local[1]]
    getdata = [remote_ds[2], remote_nods[2], local[2]]
    other = [remote_ds[0] - remote_ds[5], remote_nods[0] - remote_nods[5], local[0] - local[5]]

    return enterf, exitf, access, getdata, other

def display_total():
    fig, ax = plt.subplots()

    width = 0.25
    patterns = ('---', 'xxx', '+++', '///', 'ooo', 'OOO', '...')
    ind = np.arange(2)

    remote_nods, remote_ds, local = get_latencies()

    ax.bar(ind, remote_ds, width, color='White', hatch=patterns[0], label='Remote w/ DS')
    ax.bar(ind+width, remote_nods, width, color='White', hatch=patterns[1], label='Remote w/o DS')
    ax.bar(ind+2*width, local, width, color='White', hatch=patterns[2], label='Local')

    ax.set_xticks(ind+1.5*width)
    ax.set_xticklabels(['Read', 'Write'])

    ax.set_ylabel("Latency ($\micro$s)")
    lgd = ax.legend(bbox_to_anchor=(0.135, 0.99), loc='upper left')
    plt.tight_layout()
    format_axes(ax)
    plt.savefig("Latency_Total_Comparison.pdf", bbox_extra_artists=(lgd,), bbox_inches='tight')

def get_latencies():
    remote_ds = [all_data[0].getMedian()[0], all_data[3].getMedian()[0]]
    remote_nods = [all_data[1].getMedian()[0], all_data[4].getMedian()[0]]
    local = [all_data[2].getMedian()[0], all_data[5].getMedian()[0]]

    return remote_nods, remote_ds, local


if __name__ == '__main__':
    input_dir = sys.argv[1]
    latexify()

    if not os.path.isdir(input_dir):
        print("ERROR: takes in input directory of latency files")
        sys.exit()

    for sub_folder in os.listdir(input_dir):
        if os.path.isdir(sub_folder):
            parse_input_folder(sub_folder)

    display_total()

    display_breakdown("Read", generate_breakdown("read"))
    display_breakdown("Write", generate_breakdown("write"))