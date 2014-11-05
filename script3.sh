#!/bin/bash
./waf
./waf --run "cwnd --alpha_1=$1 --beta_1=$2 --alpha_2=$3 --beta_2=$4 --alpha_3=$5 --beta_3=$6" > cwnd.dat 2>&1
sed -rn "s/^Sender\s1\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/3/$1-$2.dat
sed -rn "s/^Sender\s2\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/3/$3-$4.dat
sed -rn "s/^Sender\s3\s(.*)\s(.*)$/\1 \2/p" cwnd.dat > data/3/$5-$6.dat
gnuplot<<__EOF
set terminal png size 1024,768
set output 'pics/3/result.png'
plot "data/3/$1-$2.dat" using 1:2 title "$1-$2" with linespoints lt 1 pt 0, "data/3/$3-$4.dat" using 1:2 title "$3-$4" with linespoints lt 3 pt 0, "data/3/$5-$6.dat" using 1:2 title "$5-$6" with linespoints lt 5 pt 0
__EOF
cp pics/3/result.png pics/3/$1,$2-$3,$4-$5,$6.png
cp goodput.dat data/3/goodput-$1,$2-$3,$4-$5,$6.dat
