import csv
import time
import operator
from igraph import *

if __name__ == '__main__':

	# Need to read and write to disk
	#reader = csv.reader(open('./twitter/twitter_rv.net', 'r'), delimiter='\t');

	#reader = csv.reader(open('./web-Google.txt', 'r'), delimiter='\t');

	reader = csv.reader(open('./inputs/simple_routes.txt', 'r'), delimiter=' ')
	
	myGraph = Graph(directed=True, vertex_attrs = {"name": []})
	myDict = dict()

	start_time = time.time()
	for row in reader:
		
		if not (row[0]) in myDict:
			myDict[row[0]] = []
		
		if not (row[1]) in myDict:
			myDict[row[1]] = []
		
		myDict[row[0]].append((row[0], row[1]))

	print "Parsing complete. Took", time.time() - start_time, "s."

	myGraph.add_vertices(myDict.keys())

	# Clean, stolen from stackoverflow
	edges = {x for v in myDict.itervalues() for x in v}
	myGraph.add_edges(edges)

	#sdsm = SharedMemorySystem(build_all_vertices(myGraph))
	
	start_time = time.time()
	
	ranks = myGraph.pagerank(directed=True, damping=0.9);
	sorted_ranks = sorted(enumerate(ranks), key = operator.itemgetter(1), reverse=True)

	print sum(ranks)
	print "PageRank complete. Took", (time.time() - start_time), "s."

	print "--------------------"
	print "Top 30 Airports..."
	print "--------------------"

	print sorted_ranks[1:30]

	print "--------------------"

