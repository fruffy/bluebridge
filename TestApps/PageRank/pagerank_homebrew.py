import time
import operator
import pdb
import graph_generator as gg

from igraph import *


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

    myGraph = Graph()
    start_time = time.time()

    if graph == "new":
        cluster_size = sys.argv[3]
        myGraph = gg.get_sample_graph(cluster_size=cluster_size, distribution=distribution)
    else:
        myGraph = Graph.Load(graph, format="ncol")
        total_nodes = len(myGraph.vs())
        myGraph["total_nodes"] = total_nodes
        myGraph.vs["rank"] = 1 / float(total_nodes)
        myGraph.vs["host"] = "m1"
        myGraph.vs["message_queue"] = [[] for _ in xrange(total_nodes)]
        myGraph.es["is_external"] = False

    print "Parsing complete. Took", time.time() - start_time, "s."

    start_time = time.time()

    hits = dict()
    ranks, hits = pagerank(myGraph)
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
