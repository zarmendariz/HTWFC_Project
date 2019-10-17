#include "board.h"
#include "experiments.h"

unsigned long long mark_reach_cycles = 0;
unsigned long long moves_cycles = 0;

void InitExperiments() {
  mark_reach_cycles = 0;
  moves_cycles = 0;
}

void PrintExperimentsStats() {
  Mprintf(1, "Total number of cycle counts for MarkReach(): %llu\n", mark_reach_cycles);
  Mprintf(1, "Total number of cycle counts for Moves(): %llu\n", moves_cycles);
}
