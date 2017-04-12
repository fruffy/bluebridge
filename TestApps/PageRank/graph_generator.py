import time
import operator
import pdb

from igraph import *

if __name__ == '__main__':

    CLUSTER_SIZE = 5000

    start_time = time.time()

    # Make a best case graph!
    g1 = Graph.Full(CLUSTER_SIZE)
    g2 = Graph.Full(CLUSTER_SIZE)

    myGraph = g1.__add__(g2)
    myGraph.add_edge(1, CLUSTER_SIZE + 1)
    myGraph.vs["host"] = (["m1"] * CLUSTER_SIZE) + (["m2"] * CLUSTER_SIZE)

    print "Graph creation complete. Took", (time.time() - start_time), "s."

    start_time = time.time()

    ranks = myGraph.pagerank(directed=False, damping=0.9)
    sorted_ranks = sorted(
        enumerate(ranks), key=operator.itemgetter(1), reverse=True)

    print sum(ranks)

    print "--------------------"
    print "Top 30 Nodes..."
    print "--------------------"

    print sorted_ranks[1:30]

    print "PageRank complete. Took", (time.time() - start_time), "s."
