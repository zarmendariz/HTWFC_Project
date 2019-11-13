#!/bin/bash

##
# run RS on a list of puzzles
# given the indices of puzzles in LIST_OF_PUZZLES
# output log will dump to LOG_DIR
##

LIST_OF_PUZZLES=experiments/can_solve.txt
LOG_DIR=experiments/log
TIME_LIMIT=${1}
COUNT=0
BIN=./RS

# the first argument is the time limit to run
if [ "$#" -ne 1 ];
then
  echo "./run.sh <time_limit>";
  exit 0;
fi

# check existence of binary
if [ ! -f "${BIN}" ]; then
  echo "Executable '${BIN}' does not exist.";
  exit 0;
fi

# check existence of output log dir
mkdir -p ${LOG_DIR}

# run RS binary on each puzzle listed in LIST_OF_PUZZLES
for i in $(cat ${LIST_OF_PUZZLES});
do
  echo "Running benchmark #${i}"
  { time timeout ${TIME_LIMIT} bash -c "echo \"S ${i}\" | ${BIN} > ${LOG_DIR}/result_${i}.log 2>&1"; } >> ${LOG_DIR}/result_${i}.log 2>&1;

  RETVAL=$?
  if [ "$RETVAL" -eq "124" ]; then
    echo " >>> Benchmark #${i} timeout";
  elif [ "$RETVAL" -eq "0" ]; then
    count=$((count + 1));
  fi
done

echo "Solved a total of ${count} benchmarks"
