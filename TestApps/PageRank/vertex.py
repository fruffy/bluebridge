import csv
import time
import operator


class SharedMemorySystem():

    def __init__(self, vertices):
        self.vertices = vertices

    def get_vertices(self):
        return self.vertices

    def message(self, source, address, value):
        self.vertices[address].add_to_queue(Message(source, value))


class Vertex():

    def __init__(self, name, edges, rank, num_nodes):

        self.incoming_message_queue = []
        self.address = name
        self.edges = edges
        self.num_nodes = num_nodes
        self.rank = rank
        self.d = 0.90

    def get_address(self):
        return self.name

    def get_rank(self):
        return self.rank

    def sendMessages(self, sdsm, t):

        outrank = self.rank / (len(self.edges) + 1)

        for edge in self.edges:
            sdsm.message(edge[0], edge[1], outrank)

        # Odds of keeping to yourself (Had to add this to prevent rank decay)
        sdsm.message(self.address, self.address, outrank)

    def add_to_queue(self, message):
        self.incoming_message_queue.append(message)

    def process(self):

        rank = 0

        for m in self.incoming_message_queue:
            rank += m.get_rank()

        self.rank = (1 - self.d) * (1 / float(self.num_nodes)) + self.d * rank
        self.incoming_message_queue = []


class Message():

    def __init__(self, src_node, score):
        self.src_node = src_node
        self.score = score

    def get_src(self):
        return self.src_node

    def get_rank(self):
        return self.score


def pagerank(sdsm):

    ranks = dict()

    # This needs to change to shared mem
    for t in range(20):

        for (_, vertex) in (sdsm.get_vertices().iteritems()):
            vertex.sendMessages(sdsm, t)

        for (_, vertex) in (sdsm.get_vertices().iteritems()):
            vertex.process()

    total_final = 0
    for label, vertex in sdsm.get_vertices().iteritems():
        total_final += vertex.get_rank()
        ranks[label] = vertex.get_rank()

    print total_final

    return sorted(ranks.items(), key=operator.itemgetter(1), reverse=True)


def build_all_vertices(graph):

    all_vertices = {}
    numNodes = len(graph)

    for label, edgelist in graph.iteritems():
        all_vertices[label] = Vertex(
            label, edgelist, 1 / float(numNodes), numNodes)

    return all_vertices

if __name__ == '__main__':

    # Need to read and write to disk
    #reader = csv.reader(open('./twitter/twitter_rv.net', 'r'), delimiter='\t');

    reader = csv.reader(open('./inputs/web-Google.txt', 'r'), delimiter='\t');

    #reader = csv.reader(open('./inputs/simple_routes.txt', 'r'), delimiter=' ')
    myGraph = dict()

    start_time = time.time()
    for row in reader:

        if not (row[0]) in myGraph:
            myGraph[row[0]] = []

        if not (row[1]) in myGraph:
            myGraph[row[1]] = []

        myGraph[row[0]].append((row[0], row[1]))

    numNodes = len(myGraph)
    print numNodes

    sdsm = SharedMemorySystem(build_all_vertices(myGraph))
    print "Parsing complete. Took", time.time() - start_time, "s."

    start_time = time.time()
    sorted_ranks = pagerank(sdsm)
    print "PageRank complete. Took", (time.time() - start_time), "s."

    print "--------------------"
    print "Top 30 Airports..."
    print "--------------------"

    print sorted_ranks[1:30]

    print "--------------------"
