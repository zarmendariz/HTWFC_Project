#!/bin/bash

if [ "$#" -ne 1 ];
then
  echo "./run.sh <ith benchmark>";
  exit 0;
fi

COUNT=0
TIME_LIMIT=${1}
BIN=./RS

if [ ! -f "${BIN}" ]; then
  echo "Executable '${BIN}' does not exist.";
  exit 0;
fi

mkdir -p log

i=$1;
echo "Running benchmark #${i}"
time bash -c "echo \"S ${i}\" | ${BIN}";
