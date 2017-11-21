set term pdf enhanced
set output "scale.pdf"

set title 'Multi-Threaded Scaling'
set ylabel 'speedup'
set xlabel 'cores'


#set arrow from 0,350 to 450,350 nohead
#set label "Cost" at 250,380

set label "12 threads" at 12,380

plot "scale.dat" using 1:3 title 'Speedup' with lines

