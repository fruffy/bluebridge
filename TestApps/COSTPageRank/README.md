# COST PageRank

This project replicated the cost single threaded page rank, and provides a faster scalable multi-threaded version with 0 COST.

## Installation

Installing PageRank requires a working [installation of Go](https://golang.org/doc/install).

Then run `go build` in PageRanks directory.

## Usage

PageRank takes an edge file as an input. The format must be [vertex][tab][vertex] where brackets are ommitied. The graph is directed, with the first vertex pointing to the second. For an example see [tiny.txt](./tiny.txt). Files are passed as command line argument with the graphFile flag.

**example:** `./PageRank -graphFile=tiny.txt`

### Flags

- **itt** itterations of page rank to compute
- **tests** number of PageRank reruns (used for benchmarking)
- **multiThread** default PageRank runs single thread use multiThread to use all cores
- **updateCache** graph files are cached after first use for speed this forces a file read
- **cpuprofile** profile PageRank with pprof


By default PageRank runs 20 itterations

## Operation

Currently PageRank is built for bench marking running with tests=5 will cause page rank to run on a single thread on test 1. 2 threads on test 2 and so forth. If anyone wants me to make this more usefull for demo purposes send me an email.

**example**

If you had the twitter_rv graph in the current directory runnint 


``./PageRank -graphFile=twitter_rv.txt -multithread=true -tests=60``

Will run 60 tests each one with one more thread on a single core, on a 60 core machine this takes roughly 4 hours.

## Performance
Single threaded pagerank is a contender against many distributed computational platforms, but is not strictly higher performance. Multi threaded PageRank has a cost of 0 against its single threaded implementation. The following graph shows how Multi threaded PageRank compaires to popular graph processing frameworks, and against the original cost threads on a 60 core server
![Performance Comparison](./plot/threaded2.pdf)

## Scalability
Multi Page Rank scales almost linearly
![Performance Comparison](./plot/scale.pdf)

