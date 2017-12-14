import time
import operator
import pdb
import sys

from igraph import *


def get_sample_graph(cluster_size=500, shape="best"):

    start_time = time.time()

    # Make a best case graph!
    if shape == "best":
        
        g1 = Graph.Full(cluster_size)
        g2 = Graph.Full(cluster_size)
        myGraph = g1.__add__(g2)

        # Add 10 edges
        for i in xrange(10):
            myGraph.add_edge(i, cluster_size + 1)

    elif shape == "dense":
        myGraph = Graph.Full(cluster_size * 2)

    total_nodes = len(myGraph.vs())
    myGraph.vs["name"] = [format(x) for x in range(cluster_size * 2)]
    myGraph["total_nodes"] = total_nodes
    myGraph.vs["rank"] = 1 / float(total_nodes)

    myGraph.vs["message_queue"] = [[] for _ in xrange(total_nodes)]
    myGraph.vs["in_edges"] = [[] for _ in xrange(total_nodes)]

    print "Graph creation complete. Took", (time.time() - start_time), "s."
    return myGraph


if __name__ == '__main__':

    print sys.argv

    if len(sys.argv) != 3:
        print "USAGE: python graph_generator.py CLUSTERSIZE"
        exit()

    cluster_size = int(sys.argv[1])
    myGraph = get_sample_graph(cluster_size=cluster_size)

    start_time = time.time()

    ranks = myGraph.pagerank(directed=False, damping=0.9)
    sorted_ranks = sorted(
        enumerate(ranks), key=operator.itemgetter(1), reverse=True)

    print "PageRank complete. Took", (time.time() - start_time), "s."

    print sum(ranks)

    print "--------------------"
    print "Top 30 Nodes..."
    print "--------------------"

    print sorted_ranks[1:30]
