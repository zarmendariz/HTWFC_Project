#!/bin/bash

##
# run RS on a specific puzzle
# take one arguemnt which is the index of the puzzle
##

if [ "$#" -ne 2 ];
then
  echo "./run_ith.sh <ith benchmark> <bin>";
  exit 0;
fi

BIN=./$2

if [ ! -f "${BIN}" ]; then
  echo "Executable '${BIN}' does not exist.";
  exit 0;
fi

i=$1;
echo "Running benchmark #${i}"
time bash -c "echo \"S ${i}\" | ${BIN}";
