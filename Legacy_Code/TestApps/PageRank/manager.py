import csv
import time
import operator
import itertools

from multiprocessing import Pool, Manager


class Vertex(object):

    def __init__(self, name, edges, rank, num_nodes):

        self.incoming_message_queue = []
        self.address = name
        self.edges = edges
        self.num_nodes = num_nodes
        self.rank = rank
        self.d = 0.90

    def get_rank(self):
        return self.rank

    def get_edges(self):
        return self.edges

    def add_to_queue(self, message):
        self.incoming_message_queue.append(message)


class Message(object):

    def __init__(self, src_node, score):
        self.src_node = src_node
        self.score = score

    def get_src(self):
        return self.src_node

    def get_rank(self):
        return self.score


def sendMessages(args):

    sdsm = args[0]
    myAddr = args[1]
    locks = args[2]

    self_vertex = sdsm[myAddr]
    myRank = self_vertex.get_rank()
    edges = self_vertex.get_edges()

    outrank = myRank / (len(edges) + 1)

    for edge in edges:

        src = edge[0]
        dst = edge[1]

        # Add a message to the destination vertex
        with locks[dst]:
            dst_vertex = sdsm[dst]
            dst_vertex.add_to_queue(Message(src, outrank))
            sdsm[dst] = dst_vertex

    # Odds of keeping to yourself (Had to add this to prevent rank decay)
    with locks[myAddr]:
        self_vertex = sdsm[myAddr]
        self_vertex.add_to_queue(Message(myAddr, outrank))
        sdsm[myAddr] = self_vertex

    if (myAddr == "YVR"):
        print "Sent", (len(edges) + 1), "messages"


def process(args):

    sdsm = args[0]
    myAddr = args[1]
    numNodes = args[2]
    locks = args[3]

    d = 0.9
    with locks[myAddr]:
        self_vertex = sdsm[myAddr]
        incoming_message_queue = self_vertex.incoming_message_queue

        if (myAddr == "YVR"):
            print "Processed", (len(self_vertex.incoming_message_queue)), "messages"

        rank = 0
        for m in self_vertex.incoming_message_queue:
            rank += m.get_rank()

        self_vertex.rank = (1 - d) * (1 / float(numNodes)) + d * rank
        self_vertex.incoming_message_queue = []
        sdsm[myAddr] = self_vertex


def pagerank(numNodes, myGraph):

    manager = Manager()
    locks = dict()

    # Creates a proxy item that is loaded/removed from shared mem
    sdsm = manager.dict()

    # Build and commit it
    build_all_vertices(sdsm, myGraph)

    # Create a dictionary of locks. One for each key
    for key in sdsm.keys():
        locks[key] = manager.Lock()

    # This needs to change to shared mem
    for t in range(2):

        process_list = []
        pool = Pool(1)
        res = pool.map(sendMessages,
                       itertools.izip(itertools.repeat(sdsm),
                                      sdsm.keys(),
                                      itertools.repeat(locks)
                                      )
                       )

        print "Rank!"

        res = pool.map(process,
                       itertools.izip(itertools.repeat(sdsm),
                                      sdsm.keys(),
                                      itertools.repeat(len(sdsm)),
                                      itertools.repeat(locks)
                                      )
                       )

    ranks = dict()
    total_final = 0
    for label in sdsm.keys():
        vertex = sdsm[label]
        total_final += vertex.get_rank()
        ranks[label] = vertex.get_rank()

    # This should be 1
    print total_final

    return sorted(ranks.items(), key=operator.itemgetter(1), reverse=True)


def build_all_vertices(sdsm, graph):

    numNodes = len(graph)

    for label, edgelist in graph.iteritems():
        sdsm[label] = Vertex(label, edgelist, 1 / float(numNodes), numNodes)

##################################
# START IT HERE
###################################

if __name__ == '__main__':

    # Need to read and write to disk

    # Huge twitter graph
    #reader = csv.reader(open('./twitter/twitter_rv.net', 'r'), delimiter='\t');

    # Medium size google graph
    reader = csv.reader(open('./web-Google.txt', 'r'), delimiter='\t')

    # super small airport graph
    #reader = csv.reader(open('./simple_routes.txt', 'r'), delimiter=' ')
    myGraph = dict()

    start_time = time.time()
    for row in reader:

        if not (row[0]) in myGraph:
            myGraph[row[0]] = []

        if not (row[1]) in myGraph:
            myGraph[row[1]] = []

        myGraph[row[0]].append((row[0], row[1]))

    print "Parsing complete. Took", time.time() - start_time, "s."

    numNodes = len(myGraph)
    print numNodes

    #sdsm = SharedMemorySystem(numNodes, build_all_vertices(myGraph))

    start_time = time.time()
    sorted_ranks = pagerank(numNodes, myGraph)
    print "PageRank complete. Took", (time.time() - start_time), "s."

    print "--------------------"
    print "Top 30 Airports..."
    print "--------------------"

    print sorted_ranks[1:30]

    print "--------------------"
