import time
import operator
import pdb
import graph_generator as gg

from igraph import *

def pagerank(myGraph, d=0.9):

    # This needs to change to shared mem
    for t in range(20):

        for vertex in (myGraph.vs()):

            outrank = vertex["rank"] / (len(vertex.neighbors()) + 1)
            vertex["message_queue"].append(outrank)

            # Send a message to all neighbors
            for nextVertex in vertex.neighbors():
                nextVertex["message_queue"].append(outrank)

        for vertex in (myGraph.vs()):
            rank_val = (1 - d) * (1 / float(myGraph["total_nodes"])) + d * sum(vertex["message_queue"])
            vertex["rank"] = rank_val
            vertex["message_queue"] = []

    return myGraph.vs["rank"]

if __name__ == '__main__':

    myGraph = gg.get_sample_graph()

    start_time = time.time()

    ranks = pagerank(myGraph)
    #ranks = myGraph.pagerank(directed=False, damping=0.9)

    sorted_ranks = sorted(
        enumerate(ranks), key=operator.itemgetter(1), reverse=True)

    print "PageRank complete. Took", (time.time() - start_time), "s."

    print sum(ranks)

    print "--------------------"
    print "Top 30 Nodes..."
    print "--------------------"

    print sorted_ranks[1:30]
