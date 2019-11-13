#include "board.h"
#include "experiments.h"

unsigned long long mark_reach_counts;
unsigned long long mark_reach_cycles;
unsigned long long mark_reach_while_counts;

unsigned long long moves_counts;
unsigned long long moves_cycles;
unsigned long long moves_while_counts;

void InitExperiments() {
  mark_reach_counts = 0;
  mark_reach_cycles = 0;
  mark_reach_while_counts = 0;

  moves_counts = 0;
  moves_cycles = 0;
  moves_while_counts = 0;
}

void PrintExperimentsStats() {
  Mprintf(1, "Total number of calling MarkReach(): %llu\n", mark_reach_counts);
  Mprintf(1, "Total number of cycles for MarkReach(): %llu\n", mark_reach_cycles);
  Mprintf(1, "Average number of cycles per call: %lf\n", mark_reach_cycles / (double)mark_reach_counts);

  Mprintf(1, "Total number of while loop for MarkReach(): %llu\n", mark_reach_while_counts);
  Mprintf(1, "Average number of cycles per loop: %lf\n", mark_reach_cycles / (double)mark_reach_while_counts);

  Mprintf(1, "Total number of calling Moves(): %llu\n", moves_counts);
  Mprintf(1, "Total number of cycles for Moves(): %llu\n", moves_cycles);
  Mprintf(1, "Average number of cycles per call: %lf\n", moves_cycles / (double)moves_counts);

  Mprintf(1, "Total number of while loop for Moves(): %llu\n", moves_while_counts);
  Mprintf(1, "Average number of cycles per loop: %lf\n", moves_cycles / (double)moves_while_counts);
}
