import csv

def ShortestPath(nodes, edges, source, dest):
    dist = {}
    prev = {}
    Q = []

    for node in nodes:
        dist[node]=9999
        prev[node]=''
        Q.append(node)

    dist[source] = 0
    minNode = popMin(dist,Q)
    while minNode != '':
        if minNode in edges:
            #print(minNode)
            for neighbour in edges[minNode]:
                alt = dist[minNode] + neighbour[1]
                #print alt
                if alt < dist[neighbour[0]]:
                    dist[neighbour[0]] = alt
                    prev[neighbour[0]] = minNode
        minNode = popMin(dist,Q)
        if len(Q) % 1000 == 0:
                print(len(Q))
        

    path = [dest]
    p = prev[dest]
    while p != source:
        path.append(p)
        p = prev[p]


    path.append(source)


    return path
    

def popMin(dist,Q):
    if len(Q) == 0:
        return ''

    minim = 9999999
    minNode = ''
    for node in Q:
        #print node
        if dist[node] < minim:
            minNode = node
            minim = dist[node]
    
    if minNode == '':
        minNode = Q[0]
    Q.remove(minNode)
    return minNode



if __name__ == '__main__':

	# Need to read and write to disk
	#reader = csv.reader(open('./twitter/twitter_rv.net', 'r'), delimiter='\t');
        #TODO use a real graph
	 ## reader = csv.reader(open('./simple_routes.txt', 'r'), delimiter=' ')
	edges = {}
	nodes = []

        reader = csv.reader(open('./web-Google.txt', 'r'), delimiter='\t')
        i=0
	for row in reader:
		if not (row[0]) in nodes:
			nodes.append(row[0])
		
		if not (row[1]) in nodes:
			nodes.append(row[1])
                
                if row[0] in edges:
                    edges[row[0]].append((row[1],1))
                else:
                    edges[row[0]] = []
                    edges[row[0]].append((row[1],1))

                if i % 10000 == 0 :
                        print ("line", i)
                i = i +1
        print "done parsing"
		

	#nodes = set(nodes);
        '''
        nodes.append('a')
        nodes.append('b')
        nodes.append('c')
        nodes.append('d')
        nodes.append('e')
        nodes.append('f')
        
        edges['a'] = []
        edges['a'].append(('b',4))
        edges['a'].append(('c',2))
        edges['b'] = []
        edges['b'].append(('c',5))
        edges['b'].append(('d',10))
        edges['c'] = []
        edges['c'].append(('e',4))
        edges['d'] = []
        edges['d'].append(('f',11))
        edges['e'] = []
        edges['e'].append(('d',4))
        '''

        sp = ShortestPath(nodes,edges,'a','d')
        print sp





