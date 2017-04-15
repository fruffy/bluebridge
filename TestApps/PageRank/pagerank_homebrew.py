import time
import operator
import pdb
import graph_generator as gg
import numpy as np
import itertools
import pymetis

from threading import Event
from multiprocessing import Pool, Manager
from igraph import *

NUM_ITERATIONS = 20

def pagerank_distributed(args, d=0.9):

    myGraph = args[0]
    shared_mem = args[1]

    remote_hits_dict = dict()

    # This needs to change to shared mem
    for t in range(NUM_ITERATIONS):

        remote_hits = 0

        for vertex in (myGraph.vs.select(state_ne="p")):

            outrank = vertex["rank"] / (len(vertex.neighbors()) + 1)
            vertex["message_queue"].append(outrank)

            # Send a message to all neighbors
            for nextVertex in vertex.neighbors():
                if (nextVertex["state"] == "p"):
                    shared_mem[(vertex["name"], nextVertex["name"], t)] = outrank
                else:
                    nextVertex["message_queue"].append(outrank)

        for vertex in (myGraph.vs.select(state_ne="p")):

            # Default amount from the random jump
            base_surfer_rank = (1 / float(myGraph["total_nodes"]))
            in_rank = sum(vertex["message_queue"])

            # For master vertices, block and wait for incoming rank
            if vertex["state"] == "m":

                total_remote_in_rank = 0

                for in_edge in vertex["in_edges"]:

                    query = in_edge + (t,)

                    # TODO: blocking
                    while shared_mem[query] == 0:
                        #print "Waiting for", (vertex["name"], t), "quick sleep"
                        time.sleep(0.005)

                    remote_hits = remote_hits + 1
                    total_remote_in_rank = total_remote_in_rank + shared_mem[query]

                in_rank = in_rank + total_remote_in_rank

            rank_val = ((1 - d) * base_surfer_rank) + (d * in_rank)
            vertex["rank"] = rank_val
            vertex["message_queue"] = []

        remote_hits_dict[t] = remote_hits

    # Return only the non proxy vertices
    return (myGraph.vs.select(state_ne="p")["rank"], remote_hits_dict)


def pagerank(myGraph, d=0.9):

    remote_hits_dict = dict()

    # This needs to change to shared mem
    for t in range(20):

        remote_hits = 0

        for vertex in (myGraph.vs()):

            outrank = vertex["rank"] / (len(vertex.neighbors()) + 1)
            vertex["message_queue"].append(outrank)

            # Send a message to all neighbors
            for nextVertex in vertex.neighbors():

                if nextVertex["host"] != vertex["host"]:
                    remote_hits = remote_hits + 1

                nextVertex["message_queue"].append(outrank)

        remote_hits_dict[t] = remote_hits

        for vertex in (myGraph.vs()):
            rank_val = (1 - d) * (1 / float(myGraph["total_nodes"])) + d * sum(vertex["message_queue"])
            vertex["rank"] = rank_val
            vertex["message_queue"] = []

    return (myGraph.vs["rank"], remote_hits_dict)


def load_graph(graphPath):

    myGraph = Graph.Load(graphPath, format="ncol")
    total_nodes = len(myGraph.vs())
    myGraph["total_nodes"] = total_nodes
    myGraph.vs["rank"] = 1 / float(total_nodes)
    myGraph.vs["host"] = 0
    myGraph.vs["message_queue"] = [[] for _ in xrange(total_nodes)]
    myGraph.vs["in_edges"] = [[] for _ in xrange(total_nodes)]
    return myGraph


def partition_graph(oneGraph, shared_mem, num_hosts=None):

    start_time = time.time()

    subgraphs = []

    num_cuts, members = pymetis.part_graph(num_hosts, oneGraph.get_adjlist())
    np_members = np.array(members)

    # Get mapping of idx to name
    name_map = np.array(oneGraph.vs["name"])

    for i in xrange(num_hosts):
        vertices = (np.where(np_members == i)[0]).tolist()
        subgraph = oneGraph.subgraph(vertices)
        subgraph.vs["state"] = "l"
        subgraphs.append(subgraph)

    num_external = 0

    print "Graph is cut.", num_cuts, "edges cut. Took", time.time() - start_time, "s."

    start_time = time.time()
    print"Adding metadata for distributed processing..."

    for edge in oneGraph.es:

        (v0, v1) = edge.tuple

        # check if external edge
        if np_members[v0] != np_members[v1]:

            num_external = num_external + 1
            v0_where = np_members[v0]
            v1_where = np_members[v1]
            v0_str = name_map[v0]
            v1_str = name_map[v1]

            # Add the external edge to v0
            vobj0 = subgraphs[v0_where].vs.select(name_eq=v0_str)[0]
            vobj0["state"] = "m"
            vobj0["in_edges"].append((v1_str, v0_str))
            subgraphs[v0_where].add_vertex(v1_str, state="p")

            # Add the external edge to v1
            vobj1 = subgraphs[v1_where].vs.select(name_eq=v1_str)[0]
            vobj1["state"] = "m"
            vobj1["in_edges"].append((v0_str, v1_str))
            subgraphs[v1_where].add_vertex(v0_str, state="p")

            subgraphs[v0_where].add_edge(v0_str, v1_str)
            subgraphs[v1_where].add_edge(v1_str, v0_str)

            # Adding vertices to shared memory. (num_expected, totalrank[])
            # Add v0
            for i in xrange(NUM_ITERATIONS):
                shared_mem[v0_str, v1_str, i] = 0
                shared_mem[v1_str, v0_str, i] = 0

    print "Cuts:", num_cuts, num_external
    print "Partitioning complete. Took", time.time() - start_time, "s."
    return subgraphs


if __name__ == '__main__':

    print sys.argv

    if len(sys.argv) < 3:
        print "USAGE: python pagerank_homebrew.py DISTRIBUTION input_file (CLUSTERS)"
        exit()

    distribution = sys.argv[1]
    graph = sys.argv[2]

    if distribution not in ['smart', 'hash']:
        print "DISTRIBUTION must be one of: smart, hash"
        exit()

    start_time = time.time()

    if graph == "new":
        cluster_size = int(sys.argv[3])
        myGraph = gg.get_sample_graph(
            cluster_size=cluster_size, distribution=distribution)
    else:
        myGraph = load_graph(graph)

    print "Parsing complete. Took", time.time() - start_time, "s."

    ####################################################
    # PROCESSING
    ####################################################

    manager = Manager()
    shared_mem = manager.dict()

    subgraphs = partition_graph(myGraph, shared_mem, num_hosts=2)

    pool = Pool(2)

    start_time = time.time()
    results = pool.map(pagerank_distributed, itertools.izip(subgraphs, itertools.repeat(shared_mem)))
    print "Distributed PageRank complete. Took", (time.time() - start_time), "s."

    hits = dict()
    ranks_dist = results[0][0] + results[1][0]
    
    hits_dist = { k: results[0][1].get(k, 0) + results[1][1].get(k, 0) for k in set(results[0][1])}

    start_time = time.time()
    ranks, hits = pagerank(myGraph)
    print "ST PageRank complete. Took", (time.time() - start_time), "s."

    # Using the igraph pagerank
    # ranks = myGraph.pagerank(directed=False, damping=0.9)

    sorted_ranks = sorted(
        enumerate(ranks_dist), key=operator.itemgetter(1), reverse=True)

    print "Total rank is", sum(ranks_dist)
    print "Remote hit rate:", hits_dist

    print "--------------------"
    print "Top 30 Nodes..."
    print "--------------------"

    print sorted_ranks[0:29]
