#ifndef STATS_H
#define STATS_H

typedef struct {
    int idle_ticks;
    int active_ticks;
    int total_ticks;
    int num_paged_in;
    int num_paged_out;
} CPUStats;

extern CPUStats stats;

void init_stats();

#endif