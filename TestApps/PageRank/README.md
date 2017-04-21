This is a basic, mock shared memory system that runs pagerank on provided graph dataset, in edgelist format.

To run, you should have virtualenv installed.

# Load up the python environment in 'PageRank' directory
virtualenv .
source ./bin/activate

# Install dependencies
pip install python-igraph
pip install numpy
pip install pymetis

# Execute a sample run...

# Generates a graph and runs PR on it
python pagerank_homebrew.py smart best 2 500  

# Try it with a real dataset, airports
python pagerank_homebrew.py smart inputs/ca-GrQc.txt 2
