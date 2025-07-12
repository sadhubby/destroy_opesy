#include "process.h"
#include "memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

Memory m;

Memory init_memory(uint64_t total_memory, uint64_t mem_per_frame, uint64_t mem_per_proc) {
    m.total_memory = total_memory;
    m.mem_per_frame = mem_per_frame;
    m.mem_per_proc = mem_per_proc;
    m.num_processes_in_memory = 0;
    m.free_memory = total_memory;
    return m;
}

// Recalculate free memory and active processes by walking the list
Memory update_free_memory(Memory mem) {
    uint64_t free_mem = 0;
    int proc_count = 0;

    MemoryBlock* curr = memory_head;
    while (curr) {
        if (!curr->occupied) {
            free_mem += (curr->end - curr->base + 1);
        } else {
            proc_count++;
        }
        curr = curr->next;
    }

    mem.free_memory = free_mem;
    mem.num_processes_in_memory = proc_count;
    return mem;
}

// Coalesce adjacent free blocks
void merge_adjacent_free_blocks(MemoryBlock **head_ref) {
    MemoryBlock* curr = *head_ref;

    while (curr && curr->next) {
        if (!curr->occupied && !curr->next->occupied) {
            // Merge next block into current
            MemoryBlock* to_delete = curr->next;
            curr->end = to_delete->end;
            curr->next = to_delete->next;
            free(to_delete);
        } else {
            curr = curr->next;
        }
    }
}

// Free a process's memory block and update memory list
Memory free_process_memory(Process *p, MemoryBlock **head_ref) {
    MemoryBlock* curr = *head_ref;

    while (curr) {
        if (curr->occupied && curr->pid == p->pid) {
            curr->occupied = false;
            curr->pid = -1;
            break;
        }
        curr = curr->next;
    }

    // Merge adjacent free blocks to reduce fragmentation
    merge_adjacent_free_blocks(head_ref);

    // Recalculate memory stats
    return update_free_memory(m);
}

MemoryBlock* init_memory_block(uint64_t total_memory) {
    MemoryBlock* head = (MemoryBlock*)malloc(sizeof(MemoryBlock));
    head->base = 0;
    head->end = total_memory - 1;
    head->occupied = false;
    head->pid = -1;
    head->next = NULL;
    return head;
}