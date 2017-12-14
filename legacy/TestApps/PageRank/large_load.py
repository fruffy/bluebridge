import sys
import pymetis
import pdb

from igraph import *

graphPath = sys.argv[1]
myGraph = Graph.Load(graphPath, format="ncol")
myGraph.write_picklez(fname="pickled.gz")
cuts, members = pymetis.part_graph(2, myGraph.get_adjlist())

pdb.set_trace()
