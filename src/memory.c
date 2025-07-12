#include "process.h"
#include "memory.h"

Memory memory;

void init_memory(uint64_t total_memory, uint64_t mem_per_frame, uint64_t mem_per_proc) {
    memory.total_memory = total_memory;
    memory.mem_per_frame = mem_per_frame;
    memory.mem_per_proc = mem_per_proc;
    memory.items = NULL;
    memory.num_processes_in_memory = 0;
}