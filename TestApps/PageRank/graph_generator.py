import time
import operator
import pdb

from igraph import *

CLUSTER_SIZE = 500


def get_sample_graph():

    start_time = time.time()

    # Make a best case graph!
    g1 = Graph.Full(CLUSTER_SIZE)
    g2 = Graph.Full(CLUSTER_SIZE)

    myGraph = g1.__add__(g2)

    total_nodes = len(myGraph.vs())
    myGraph["total_nodes"] = total_nodes
    myGraph.vs["rank"] = 1 / float(total_nodes)
    myGraph.vs["host"] = (["m1"] * CLUSTER_SIZE) + (["m2"] * CLUSTER_SIZE)
    myGraph.vs["message_queue"] = [[] for _ in xrange(total_nodes)]

    myGraph.es["is_external"] = False
    myGraph.add_edge(1, CLUSTER_SIZE + 1, is_external=True)

    print "Graph creation complete. Took", (time.time() - start_time), "s."
    return myGraph


if __name__ == '__main__':

    myGraph = get_sample_graph()

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
