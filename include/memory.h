#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

typedef struct Memory {
    uint64_t total_memory;
    uint64_t mem_per_frame;
    uint64_t mem_per_proc;
    uint64_t free_memory;
    int num_processes_in_memory;
} Memory;

typedef struct MemoryBlock {
    int base;
    int end;
    bool occupied;
    int pid;
    struct MemoryBlock* next;
} MemoryBlock;

// Global memory pointer
extern MemoryBlock* memory_head;

// Core memory functions
Memory init_memory(uint64_t total_memory, uint64_t mem_per_frame, uint64_t mem_per_proc);
Memory update_free_memory(Memory mem);
Memory free_process_memory(Process *p, MemoryBlock **head_ref);
MemoryBlock* init_memory_block(uint64_t total_memory);
void merge_adjacent_free_blocks(MemoryBlock **head_ref);
void write_memory_snapshot(int quantum_cycle, MemoryBlock *memory_head);

// vmstat and process-smi
void handle_process_smi(MemoryBlock *memory_head, Memory *mem);
// void handle_vmstat(Memory *mem, CPUStats *stats);
#endif
