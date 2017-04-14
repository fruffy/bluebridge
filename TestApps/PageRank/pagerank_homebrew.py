import time
import operator
import pdb
import graph_generator as gg
import numpy as np
import itertools

from threading import Event
from multiprocessing import Pool, Manager
from igraph import *


def pagerank_distributed(args, d=0.9):

    myGraph = args[0]
    shared_mem = args[1]

    remote_hits_dict = dict()

    # This needs to change to shared mem
    for t in range(20):

        remote_hits = 0

        for vertex in (myGraph.vs.select(state_ne="p")):

            outrank = vertex["rank"] / (len(vertex.neighbors()) + 1)
            vertex["message_queue"].append(outrank)

            # Send a message to all neighbors
            for nextVertex in vertex.neighbors():
                if (nextVertex["state"] == "p"):
                    # print vertex["name"], "needs to send to",
                    # nextVertex["name"], "at", t
                    remote_hits = remote_hits + 1
                    shared_mem[(nextVertex["name"], t)] = outrank
                else:
                    nextVertex["message_queue"].append(outrank)

        remote_hits_dict[t] = remote_hits

        for vertex in (myGraph.vs.select(state_ne="p")):

            # Default amount from the random jump
            base_surfer_rank = (1 / float(myGraph["total_nodes"]))
            in_rank = sum(vertex["message_queue"])

            # For master vertices, block and wait for incoming rank
            if vertex["state"] == "m":

                # TODO: blocking
                while (vertex["name"], t) not in shared_mem:
                    print "Waiting for", (vertex["name"], t), "quick sleep"
                    time.sleep(0.01)

                in_rank = in_rank + shared_mem[(vertex["name"], t)]
                #print vertex["name"], shared_mem[(vertex["name"], t)], "from", t

            rank_val = ((1 - d) * base_surfer_rank) + (d * in_rank)
            vertex["rank"] = rank_val
            vertex["message_queue"] = []

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
    return myGraph


def partition_graph(oneGraph, num_hosts=None):

    start_time = time.time()

    subgraphs = []
    cutObject = oneGraph.st_mincut(1, len(oneGraph.vs) - 1)

    for vertices in cutObject.partition:
        subgraph = oneGraph.subgraph(vertices)
        subgraph.vs["state"] = "l"

        for edge in cutObject.es:

            (v0, v1) = edge.tuple

            if (v1 not in vertices):
                subgraph.vs.select(name_eq=str(v0))["state"] = "m"
                subgraph.add_vertex(str(v1), state="p")
            elif (v0 not in vertices):
                subgraph.vs.select(name_eq=str(v1))["state"] = "m"
                subgraph.add_vertex(str(v0), state="p")
            else:
                print "SOMETHING IS WRONG"

            subgraph.add_edge(str(edge.tuple[0]), str(edge.tuple[1]))

        subgraphs.append(subgraph)

    # Partitioning is hardcoded for now.
    '''
    np_hosts = np.array(myGraph.vs["host"])
    for i in xrange(num_hosts):
        vertices = np.where(np_hosts == i)[0].tolist()
        subgraph = oneGraph.subgraph(vertices)
        subgraph.vs["is_proxy"] = False
        subgraphs.append(subgraph)
    '''

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
        myGraph = load_graph(graphPath)

    print "Parsing complete. Took", time.time() - start_time, "s."

    ####################################################
    # PROCESSING
    ####################################################
    start_time = time.time()

    manager = Manager()
    pool = Pool(2)
    subgraphs = partition_graph(myGraph, num_hosts=2)

    shared_mem = manager.dict()
    results = pool.map(pagerank_distributed,itertools.izip(subgraphs, itertools.repeat(shared_mem)))

    print "Distributed PageRank complete. Took", (time.time() - start_time), "s."

    start_time = time.time()
    
    hits = dict()
    ranks, hits = pagerank(myGraph)
    ranks_dist = results[0][0] + results[1][0]

    # Using the igraph pagerank
    #ranks = myGraph.pagerank(directed=False, damping=0.9)

    sorted_ranks = sorted(
        enumerate(ranks_dist), key=operator.itemgetter(1), reverse=True)

    print "ST PageRank complete. Took", (time.time() - start_time), "s."

    print "Total rank is", sum(ranks_dist)
    print "Remote hit rate:", hits

    print "--------------------"
    print "Top 30 Nodes..."
    print "--------------------"

    print sorted_ranks[1:30]
