reset
set title "Time cost"
set xlabel "n th sequence of Fibonacci number"
set ylabel "time cost (ns)"
set terminal png font " Times_New_Roman,12 "
set output "time.png"
set xrange [0:1000]
set xtics 0, 100, 1000
set key left

plot \
"time/time" using 1:2 with linespoints linewidth 2 title "original", \
"time/time" using 1:3 with linespoints linewidth 2 title "bn_add", \
"time/time" using 1:4 with linespoints linewidth 2 title "bn_fast-doubling", \