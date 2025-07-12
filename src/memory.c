#include "process.h"
#include "memory.h"

Memory m;

Memory init_memory(uint64_t total_memory, uint64_t mem_per_frame, uint64_t mem_per_proc) {
    m.total_memory = total_memory;
    m.mem_per_frame = mem_per_frame;
    m.mem_per_proc = mem_per_proc;
    m.num_processes_in_memory = 0;
    m.free_memory = total_memory;
    return m;
}

Memory update_free_memory(Memory new_memory) { 
    
    new_memory.free_memory -= new_memory.mem_per_proc;
    new_memory.num_processes_in_memory++;
    return new_memory;
}