#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "stats.h"
#include "process.h"

typedef struct Memory {
    uint64_t total_memory;
    uint64_t mem_per_frame;
    uint64_t max_mem_per_proc;
    uint64_t min_mem_per_proc;
    uint64_t used_memory;
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


extern Memory memory;
extern MemoryBlock* memory_head;

// Read a uint16 value from memory for a given process
uint16_t read_from_memory(Process *p, uint16_t addr);

// Write a uint16 value to memory for a given process
void write_to_memory(Process *p, uint16_t addr, uint16_t value);

// Core memory functions
void init_memory(uint64_t total_memory, uint64_t mem_per_frame, uint64_t max_mem_per_proc, uint64_t min_mem_per_proc);
void update_free_memory();
void free_process_memory(Process *p, MemoryBlock **head_ref);
MemoryBlock* init_memory_block(uint64_t total_memory);
void merge_adjacent_free_blocks(MemoryBlock **head_ref);

// vmstat and process-smi
void process_smi(int num_cores, Process **cpu_cores);
void vmstat(Memory *mem, CPUStats *stats);

int memory_write(uint32_t address, uint16_t value, Process *p);
uint16_t memory_read(uint32_t address, Process *p, int *success);
#endif
