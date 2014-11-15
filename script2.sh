#!/bin/bash
./waf
./waf --run "cwnd --alpha_1=$1 --beta_1=$2 --alpha_2=$3 --beta_2=$4" > cwnd.dat 2>&1
sed -rn "s/^Sender\s1\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/2/$1-$2.dat
sed -rn "s/^Sender\s2\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/2/$3-$4.dat
gnuplot<<__EOF
set terminal png size 1024,768
set output 'pics/2/result.png'
plot "data/2/$1-$2.dat" using 1:2 title "$1-$2" with linespoints lt 1 pt 0, "data/2/$3-$4.dat" using 1:2 title "$3-$4" with linespoints lt 3 pt 0
__EOF
cp pics/2/result.png pics/2/$1-$2-vs-$3-$4.png
