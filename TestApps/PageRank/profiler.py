import pstats
import pdb

p = pstats.Stats("stats.out")
p.strip_dirs().sort_stats('time').print_stats(10)

pdb.set_trace()