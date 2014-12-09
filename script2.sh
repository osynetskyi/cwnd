#!/bin/bash
./waf
./waf --run "cwnd --alpha_1=$1 --alpha_2=$2 --bufsize=$3" > cwnd.dat 2>&1
sed -rn "s/^Sender\s1\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/2/$1@$3.dat
sed -rn "s/^Sender\s2\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/2/$2@$3.dat
sed -rn "s/^Approx\.\sthroughput\s1\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/2/tp-$1@$3.dat
sed -rn "s/^Approx\.\sthroughput\s2\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/2/tp-$2@$3.dat
gnuplot<<__EOF
set terminal png size 1024,768
set output 'pics/2/result.png'
plot "data/2/$1@$3.dat" using 1:2 title "$1 @ $3" with linespoints lt 1 pt 0, "data/2/$2@$3.dat" using 1:2 title "$2 @ $3" with linespoints lt 3 pt 0
__EOF
gnuplot<<__EOF
set terminal png size 1920,1080
set output 'pics/2/tp-result.png'
plot "data/2/tp-$1@$3.dat" using 1:2 title "$1 @ $3" with linespoints lt 1 pt 0, "data/2/tp-$2@$3.dat" using 1:2 title "$2 @ $3" with linespoints lt 3 pt 0
__EOF
cp pics/2/result.png pics/2/$1-vs-$2@$3.png
cp pics/2/tp-result.png pics/2/tp-$1-vs-$2@$3.png
cp goodput.dat data/2/goodput-$1-vs$2@$3.dat
