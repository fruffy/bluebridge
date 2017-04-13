import csv
import time
import operator
import numpy as np
import pdb

from sklearn.cluster import KMeans
from igraph import *

if __name__ == '__main__':

    # Need to read and write to disk
    #reader = csv.reader(open('./twitter/twitter_rv.net', 'r'), delimiter='\t');

    #reader = csv.reader(open('./web-Google.txt', 'r'), delimiter='\t');

    #reader = csv.reader(open('./inputs/simple_routes.txt', 'r'), delimiter=' ')

    inputFile = './inputs/web-Google.txt'
    NUM_NODES = 10

    start_time = time.time()
    myGraph = Graph.Load(inputFile, format="edge")
    print "Parsing complete. Took", time.time() - start_time, "s."

    '''
    # Try a KMeans Method for partitioning
    start_time = time.time()

    # Convert the graph into a Euclidean space via MDS.
    layout = myGraph.layout_mds()
    clusters = KMeans(n_clusters=NUM_NODES).fit(np.array(layout.coords))

    subGraphs = []

    for i in range(NUM_NODES):
        subGraph = myGraph.subgraph((np.where(clusters.labels_ == i)[0]).tolist())
        subGraphs.append(subGraph)
        print "Cluster", i, "has", len(subGraph.vs)

    print "KMeans MDS partitioning complete. Took", time.time() - start_time, "s."
    '''
    '''
    start_time = time.time()

    graphPieces = dict()
    maxGraph = myGraph

    while len(graphPieces.keys()) < 10:
        cut = maxGraph.mincut()
        graphPieces[maxGraph.subgraph(cut.partition[0])] = len(cut.partition[0])
        graphPieces[maxGraph.subgraph(cut.partition[1])] = len(cut.partition[1])
        maxGraph = max(graphPieces.keys(), key=(lambda key: graphPieces[key]))
        graphPieces.pop(maxGraph, None)
        print graphPieces

    print "Greedy mincut partitioning complete. Took", time.time() - start_time, "s."
    
    for key in graphPieces.keys():
        print "Cluster has", len(key.vs)
    '''

    start_time = time.time()

    ranks = myGraph.pagerank(directed=True, damping=0.9)
    sorted_ranks = sorted(
        enumerate(ranks), key=operator.itemgetter(1), reverse=True)

    print sum(ranks)
    print "PageRank complete. Took", (time.time() - start_time), "s."

    print "--------------------"
    print "Top 30 Airports..."
    print "--------------------"

    print sorted_ranks[1:30]

    print "--------------------"
