#!/bin/bash

if [ "$#" -ne 1 ];
then
  echo "./run.sh <time_limit>";
  exit 0;
fi

COUNT=0
TIME_LIMIT=${1}
BIN=./RS_O1

if [ ! -f "${BIN}" ]; then
  echo "Executable '${BIN}' does not exist.";
  exit 0;
fi

mkdir -p log

for i in $(seq 1 91);
do
  echo "Running benchmark #${i}"
  timeout ${TIME_LIMIT} bash -c "echo \"S ${i}\" | ${BIN} > log/result_${i}.log 2>&1";
  RETVAL=$?
  if [ "$RETVAL" -eq "124" ]; then
    echo " >>> Benchmark #${i} timeout";
  else
    count=$((count + 1));
  fi
done

echo "Solved a total of ${count} benchmarks"