set term pdf enhanced
set output "threaded.pdf"

set title 'COST vs Multi Threaded PageRank'
set ylabel 'Time (S)'
set xlabel '# of CPU threads'


set arrow from 0,350 to 500,350 nohead
set label "Cost" at 5,380

plot "threaded2.dat" using 2:3 title 'Multi-Threaded Pagerank' with lines

