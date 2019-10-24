#ifndef __EXPERIMENTS_H
#define __EXPERIMENTS_H

unsigned long long mark_reach_counts;
unsigned long long mark_reach_cycles;
unsigned long long mark_reach_while_counts;

unsigned long long moves_counts;
unsigned long long moves_cycles;
unsigned long long moves_while_counts;

void InitExperiments();
void PrintExperimentsStats();

#endif
