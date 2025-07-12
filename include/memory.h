#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <time.h>
#include "process.h"

typedef struct {
    uint64_t total_memory;
    uint64_t mem_per_frame;
    uint64_t mem_per_proc;
    Process **items;
    int num_processes_in_memory;
    time_t last_memory_time;
} Memory;

#endif
