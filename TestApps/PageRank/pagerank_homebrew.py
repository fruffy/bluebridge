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

        for vertex in (myGraph.vs.select(is_proxy_eq=False)):

            outrank = vertex["rank"] / (len(vertex.neighbors()) + 1)
            vertex["message_queue"].append(outrank)

            # Send a message to all neighbors
            for nextVertex in vertex.neighbors():
                if (nextVertex["is_proxy"]):
                    print vertex["name"], "needs to send to", nextVertex["name"], "at", t
                    remote_hits = remote_hits + 1
                    shared_mem[(nextVertex["name"], t)] = outrank
                else:
                    nextVertex["message_queue"].append(outrank)

        remote_hits_dict[t] = remote_hits

        for vertex in (myGraph.vs.select(is_proxy_eq=False)):
            
            # Default amount from the random jump
            base_surfer_rank = (1 / float(myGraph["total_nodes"]))
            in_rank = sum(vertex["message_queue"])

            '''
            if t > 0:

                # TODO: blocking and is_proxied
                print shared_mem
                while (vertex["name"], t-1) not in shared_mem:
                    print "Waiting for", (vertex["name"], t-1) ,"quick sleep"
                    time.sleep(0.01)

                in_rank = in_rank + (shared_mem[(vertex["name"], t-1)])
            '''

            rank_val = ((1-d) * base_surfer_rank) + (d * in_rank)
            vertex["rank"] = rank_val
            vertex["message_queue"] = []
    
    return (myGraph.vs.select(is_proxy_eq=False)["rank"], remote_hits_dict)


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
            rank_val = (
                1 - d) * (1 / float(myGraph["total_nodes"])) + d * sum(vertex["message_queue"])
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

    subgraphs = []
    cutObject = oneGraph.st_mincut(1, len(oneGraph.vs) - 1)

    for vertices in cutObject.partition:
        subgraph = oneGraph.subgraph(vertices)
        subgraph.vs["is_proxy"] = False

        for edge in cutObject.es:

            if (edge.tuple[0] not in vertices):
                subgraph.add_vertex(str(edge.tuple[0]), is_proxy=True)
            elif (edge.tuple[1] not in vertices):
                subgraph.add_vertex(str(edge.tuple[1]), is_proxy=True)
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
    results = pool.map(pagerank_distributed,
                       itertools.izip(subgraphs, itertools.repeat(shared_mem))
                       )

    hits = dict()

    ranks, hits = pagerank(myGraph)
    ranks_dist = results[0][0] + results[1][0]

    # Using the igraph pagerank
    #ranks = myGraph.pagerank(directed=False, damping=0.9)

    sorted_ranks = sorted(
        enumerate(ranks), key=operator.itemgetter(1), reverse=True)

    print "PageRank complete. Took", (time.time() - start_time), "s."

    print "Total rank is", sum(ranks)
    print "Remote hit rate:", hits

    print "--------------------"
    print "Top 30 Nodes..."
    print "--------------------"

    print sorted_ranks[1:30]
