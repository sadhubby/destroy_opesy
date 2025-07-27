#include "stats.h"

CPUStats stats;

void init_stats() {
    stats.active_ticks = 0;
    stats.idle_ticks = 0;
    stats.num_paged_in = 0;
    stats.num_paged_out = 0;
    stats.total_ticks = 0;
}