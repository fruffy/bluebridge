set term pdf enhanced
set output "threadedlog.pdf"


set logscale xy
set title 'Performance compairson'
set ylabel 'time (s)'
set xlabel 'cores'


set arrow from 1,350 to 1000,350 nohead
set label "Cost" at 2,400

plot 'threaded2.dat' using 2:3 title 'Multi-Threaded Pagerank' with lines, \
    'frameworks.dat' using 2:3 title 'Framework', \
    'frameworks.dat' using 2:3:1 with labels title 'Pagerank'


