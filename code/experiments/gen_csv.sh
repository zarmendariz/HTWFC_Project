#!/bin/bash
LIST_OF_PUZZLES=can_solve.txt

echo "Name,Moves,Pushes,Treedepth,MarkReach calls,MarkReach cycles,Avg cycles/call,loops,Approx cycles/loop,Stones loops,Avg stones,unused,unused,unused,total time(s)";
for n in $(cat ${LIST_OF_PUZZLES});
do
  printf "Puzzle_%s", $n;
  cat log/result_${n}.log | tail -n 16 | head -n 14 |\
    awk '{if (NR > 2 && NR != 13 && NR != 14) printf "%s,", $NF; else if (NR == 2) printf "%s%s%s,", $2, $4, $6; else if (NR == 14) printf "%s", substr($NF,3,length($NF)-3) }';
  echo "";
done
