reset
set title "Time cost"
set xlabel "n th sequence of Fibonacci number"
set ylabel "time cost (ns)"
set terminal png font " Times_New_Roman,12 "
set output "time_page3.png"
set xrange [0:1000]
set xtics 0, 100, 1000
set key left

plot \
"time/time" using 1:2 with linespoints linewidth 2 title "bn\_add", \
"time/time" using 1:3 with linespoints linewidth 2 title "bn\_fast-doubling", \
