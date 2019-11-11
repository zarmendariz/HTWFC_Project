#!/bin/bash
NAME=summary.txt
echo "" > $NAME
for name in $(ls profile/*);
do
  # echo "$name" >> $NAME
  # head -n 10 $name | awk '{ if (NR > 5) { if (FNR >= 6) printf "\"%s\", ", $7; else printf "\"0\", "; } }' >> $NAME;
  head -n 10 $name | awk '{ if (NR > 5) { if (FNR >= 6) printf "{\"%s\": %s}, ", $7, $3; else printf "\"0\", "; } }' >> $NAME;
  echo "" >> $NAME
done
