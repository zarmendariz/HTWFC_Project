#!/bin/bash

if [ "$#" -ne 1 ];
then
  echo "./run_prof.sh <time_limit>";
  exit 0;
fi

COUNT=0
TIME_LIMIT=${1}
BIN=./RS_PG

if [ ! -f "${BIN}" ]; then
  echo "Executable '${BIN}' does not exist.";
  exit 0;
fi

mkdir -p profile

for i in $(seq 1 91);
do
  echo "Running benchmark #${i}"
  echo "S ${i}" > tmp
  echo "Q" >> tmp
  timeout ${TIME_LIMIT} bash -c "cat tmp | ${BIN}";
  RETVAL=$?
  if [ "$RETVAL" -eq "124" ]; then
    echo " >>> Benchmark #${i} timeout";
  else
    gprof ${BIN} > profile/profile_${i}.txt
    count=$((count + 1));
  fi
done

echo "Solved a total of ${count} benchmarks"
