This is a basic, mock shared memory system that runs pagerank on provided graph dataset, in edgelist format.

To run, you should have virtualenv installed.

# Load up the python environment in 'PageRank' directory
```
virtualenv .
source ./bin/activate
```

# Install dependencies
```
pip install python-igraph
pip install numpy
pip install pymetis
```

# Execute a sample run...

This generates a graph and runs PageRank on it:

```
python pagerank_homebrew.py smart best 2 500  
```

Or, try it with a real dataset from the presented paper

```
python pagerank_homebrew.py smart inputs/ca-GrQc.txt 2
```

# Command Arguments

python pagerank_homebrew.py <PARTITION> <INPUT> <NUM_CORES>  

PARTITION corresponds to either 'best' for ParMetis, or 'rand' for random assignment.
INPUT is either a path to the input edgelist file, or 'best' for a generated best case graph, 'dense' for a worst case fully connected graph.
NUM_CORES is the numer of parallel processes to use.