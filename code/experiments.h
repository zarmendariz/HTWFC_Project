#ifndef __EXPERIMENTS_H
#define __EXPERIMENTS_H

extern unsigned long long mark_reach_counts;
extern unsigned long long mark_reach_cycles;
extern unsigned long long mark_reach_while_counts;

extern unsigned long long moves_counts;
extern unsigned long long moves_cycles;
extern unsigned long long moves_while_counts;

void InitExperiments();
void PrintExperimentsStats();

#endif
