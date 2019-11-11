## Current Structure of Experiments

- Functions and Variables are defined in following files:
```
experiments.c
experiments.h
```

- The target functions:
  1. `mark.c::MarkReach` has been renamed to `MarkReachImpl`
  2. `moves.c::Moves` has been renamed to `MovesImpl`

## How to run the experiments

- `./run_ith.sh <puzzle no.>` to run the ith puzzle

- `./run.sh <time limit in second>` to run all 42 puzzles defined in `experiments/can_solve.txt` with a time limit

The log will be written to `experiments/log/result_<i>.log`.

- Run `./gen_csv.sh` under `experiments/` directory can generate a csv format output parsing from `experiments/log`
