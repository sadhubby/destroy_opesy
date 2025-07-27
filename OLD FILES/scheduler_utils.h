#ifndef SCHEDULER_UTILS_H
#define SCHEDULER_UTILS_H

#include "scheduler.h"

void create_process(Process *proc, int idx, int burst);

int get_random_burst(int min, int max);

int find_free_process_slot(void);

extern int global_process_counter;

#endif