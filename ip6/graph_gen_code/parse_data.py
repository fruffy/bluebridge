#!/usr/bin/env python

"""
parse_data.py

Created by Amanda Carbonari on 10/20/2016.
"""

import os
import sys
import re
import numpy as np
import math

class data_struct:

    def __init__ (self, size):
        self.size = size
        self.c_rpc_start = np.zeros(0)
        self.c_rpc_end = np.zeros(0)
        self.c_send = np.zeros(0)
        self.c_recv = np.zeros(0)
        self.s_rpc_start = np.zeros(0)
        self.s_rpc_end = np.zeros(0)
        self.s_send = np.zeros(0)
        self.s_recv = np.zeros(0)

def populate_array(test, method, count, dir):
    server = "s2"
    client = "c1"
    offset = 0

    if "add" in method:
        offset = 2200

    power = math.log(count, 2)*100
    d = data_struct(count)
    d.size = count

    #server
    filename = "%s_%s_%s-%s_%d.txt" % (test, method, server, "rpc-start", count)
    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        d.s_rpc_start = np.append(d.s_rpc_start, long(line))

    fin.close()
    filename = "%s_%s_%s-%s_%d.txt" % (test, method, server, "rpc-end", count)
    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        d.s_rpc_end = np.append(d.s_rpc_end, long(line))

    fin.close()

    i = 0
    filename = "%s_%s-%s.txt" % (test, server, "send")
    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        if i >= power+offset and i < power+offset + 100:
            d.s_send = np.append(d.s_send, long(line))
        i+=1

    fin.close()

    i = 0
    filename = "%s_%s-%s.txt" % (test, server, "recv")
    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        if i >= power+offset and i < power+offset + 100:
            d.s_recv = np.append(d.s_recv, long(line))
        i+=1

    #client
    fin.close()

    if "bifrost" in test:
        filename = "%s_%s_%s_%d.txt" % (test, method, "rpc-start", count)
    else:
        filename = "%s_%s_%s-%s_%d.txt" % (test, method, client, "rpc-start", count)

    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        d.c_rpc_start = np.append(d.c_rpc_start, long(line))

    fin.close()

    if "bifrost" in test:
        filename = "%s_%s_%s_%d.txt" % (test, method, "rpc-end", count)
    else:
        filename = "%s_%s_%s-%s_%d.txt" % (test, method, client, "rpc-end", count)

    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        d.c_rpc_end = np.append(d.c_rpc_end, long(line))

    fin.close()
    filename = "%s_%s_%s-%s_%d.txt" % (test, method, client, "send", count)
    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        d.c_send = np.append(d.c_send, long(line))

    fin.close()
    filename = "%s_%s_%s-%s_%d.txt" % (test, method, client, "recv", count)
    fin = open(os.path.join(dir, filename), "r")

    for line in fin:
        d.c_recv = np.append(d.c_recv, long(line))
    fin.close()

    return d

if __name__ == '__main__':
    if len(sys.argv) != 2:
        usage()
        sys.exit(-1)

    input_thing = sys.argv[1]
    thrift_incr_arr_file = "./thrift_incr-arr_breakdown.csv"
    bifrost_incr_arr_file = "./bifrost_incr-arr_breakdown.csv"
    thrift_add_arr_file = "./thrift_add-arr_breakdown.csv"
    bifrost_add_arr_file = "./bifrost_add-arr_breakdown.csv"
    max_power = 22

    input_dir = os.path.abspath(input_thing)

    all_data = {}

    for test in ["bifrost", "thrift"]:
        all_data[test] = {}
        for method in ["incr-arr", "add-arr"]:
            all_data[test][method] = {}
            for count in range(0, max_power):
                size = 2**count
                # print "Parsing: %d, %s, %s" % (size, test, method)
                all_data[test][method][size] = populate_array(test, method, size, input_dir)

    fout = open(bifrost_incr_arr_file, "w")
    fout.write("data size,client marshalling,c to s network,server unmarshalling,server comp,server marshalling,s to c network,client unmarshalling\n")
    for count in range(0, max_power):
        size = 2**count
        print "Generating: %d, %s, %s" % (size, "bifrost", "incr-arr")
        value = all_data["bifrost"]["incr-arr"][size]
        cmar = np.mean(value.c_send - value.c_rpc_start)
        c2snet = np.mean(value.s_recv - value.c_send)
        sunmar = np.mean(value.s_rpc_start - value.s_recv)
        sercomp = np.mean(value.s_rpc_end - value.s_rpc_start)
        sermar = np.mean(value.s_send - value.s_rpc_end)
        s2cnet = np.mean(value.c_recv - value.s_send)
        cunmar = np.mean(value.c_rpc_end - value.c_recv)
        line = "%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n" % (size, cmar, c2snet, sunmar, sercomp, sermar, s2cnet, cunmar)
        fout.write(line)
    fout.close()

    fout = open(thrift_incr_arr_file, "w")
    fout.write("data size,client marshalling,c to s network,server unmarshalling,server comp,server marshalling,s to c network,client unmarshalling\n")
    for count in range(0, max_power):
        size = 2**count
        print "Generating: %d, %s, %s" % (size, "bifrost", "incr-arr")
        value = all_data["thrift"]["incr-arr"][size]
        cmar = np.mean(value.c_send - value.c_rpc_start)
        c2snet = np.mean(value.s_recv - value.c_send)
        sunmar = np.mean(value.s_rpc_start - value.s_recv)
        sercomp = np.mean(value.s_rpc_end - value.s_rpc_start)
        sermar = np.mean(value.s_send - value.s_rpc_end)
        s2cnet = np.mean(value.c_recv - value.s_send)
        cunmar = np.mean(value.c_rpc_end - value.c_recv)
        line = "%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n" % (size, cmar, c2snet, sunmar, sercomp, sermar, s2cnet, cunmar)
        fout.write(line)
    fout.close()

    fout = open(bifrost_add_arr_file, "w")
    fout.write("data size,client marshalling,c to s network,server unmarshalling,server comp,server marshalling,s to c network,client unmarshalling\n")
    for count in range(0, max_power):
        size = 2**count
        print "Generating: %d, %s, %s" % (size, "bifrost", "incr-arr")
        value = all_data["bifrost"]["add-arr"][size]
        cmar = np.mean(value.c_send - value.c_rpc_start)
        c2snet = np.mean(value.s_recv - value.c_send)
        sunmar = np.mean(value.s_rpc_start - value.s_recv)
        sercomp = np.mean(value.s_rpc_end - value.s_rpc_start)
        sermar = np.mean(value.s_send - value.s_rpc_end)
        s2cnet = np.mean(value.c_recv - value.s_send)
        cunmar = np.mean(value.c_rpc_end - value.c_recv)
        line = "%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n" % (size, cmar, c2snet, sunmar, sercomp, sermar, s2cnet, cunmar)
        fout.write(line)
    fout.close()

    fout = open(thrift_add_arr_file, "w")
    fout.write("data size,client marshalling,c to s network,server unmarshalling,server comp,server marshalling,s to c network,client unmarshalling\n")
    for count in range(0, max_power):
        size = 2**count
        print "Generating: %d, %s, %s" % (size, "bifrost", "incr-arr")
        value = all_data["thrift"]["add-arr"][size]
        cmar = np.mean(value.c_send - value.c_rpc_start)
        c2snet = np.mean(value.s_recv - value.c_send)
        sunmar = np.mean(value.s_rpc_start - value.s_recv)
        sercomp = np.mean(value.s_rpc_end - value.s_rpc_start)
        sermar = np.mean(value.s_send - value.s_rpc_end)
        s2cnet = np.mean(value.c_recv - value.s_send)
        cunmar = np.mean(value.c_rpc_end - value.c_recv)
        line = "%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n" % (size, cmar, c2snet, sunmar, sercomp, sermar, s2cnet, cunmar)
        fout.write(line)
    fout.close()

    # for test, m_data in all_data.iteritems():
    #   print "Test: %s" % test
    #   for method, d_data in m_data.iteritems():
    #       print "\tMethod: %s" % method
    #       for key, value in d_data.iteritems():
    #           print "\tSize: %d" % key
    #           print "\t\tC-->S: down (%d), network (%d), up (%d)" % (np.mean(value.c_send - value.c_rpc_start), np.mean(value.s_recv - value.c_send), np.mean(value.s_rpc_end - value.s_recv))
    #           print "\t\tS-->C: down (%d), network (%d), up (%d)" % (np.mean(value.s_send - value.s_rpc_end), np.mean(value.c_recv - value.s_send), np.mean(value.c_rpc_end - value.c_recv))
