#!/bin/bash

mkdir -p log

for i in $(seq 1 91);
do
  echo "Running benchmark #${i}"
  timeout 50 bash -c "echo \"S ${i}\" | ./RS > log/result_${i}.log 2>&1";
  RETVAL=$?
  if [ "$RETVAL" -eq "124" ]; then
    echo "Benchmark #${i} timeout"
  fi
done

