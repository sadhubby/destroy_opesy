#include "scheduler_utils.h"
#include <stdio.h>
#include <stdlib.h>

void create_process(Process *proc, int idx, int burst) {
    proc->burst_time = burst;
    proc->total_prints = burst;
    proc->finished_print = 0;
    proc->is_finished = 0;
    proc->core_assigned = -1;
    proc->start_time = 0;
    proc->end_time = 0;
}

int get_random_burst(int min, int max) {
    return min + rand() % (max - min + 1);
}

int find_free_process_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].burst_time == 0)
            return i;
    }
    return -1;
}

int global_process_counter = 0;