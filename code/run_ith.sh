#!/bin/bash

##
# run RS on a specific puzzle
# take one arguemnt which is the index of the puzzle
##

BIN=./RS

if [ "$#" -ne 1 ];
then
  echo "./run.sh <ith benchmark>";
  exit 0;
fi

if [ ! -f "${BIN}" ]; then
  echo "Executable '${BIN}' does not exist.";
  exit 0;
fi

i=$1;
echo "Running benchmark #${i}"
time bash -c "echo \"S ${i}\" | ${BIN}";
