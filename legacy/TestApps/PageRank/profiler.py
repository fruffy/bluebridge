import pstats
import pdb
import sys

p = pstats.Stats(sys.argv[1])
p.strip_dirs().sort_stats('time').print_stats(10)

pdb.set_trace()