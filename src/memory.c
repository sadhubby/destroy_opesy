#include "process.h"
#include "memory.h"
#include "scheduler.h"
#include "stats.h"
#include "backing_store.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAX_MEMORY_SIZE 16384  // 16 KB
#define UINT16_MAX_VAL 65535

static uint16_t memory_space[MAX_MEMORY_SIZE / 2];

Memory memory;
MemoryBlock* memory_head;

// frame table
#define MAX_FRAMES 1024

// Write a uint16 value to memory for a given process
void write_to_memory(Process *p, uint16_t addr, uint16_t value) {
    // Check if address is valid for this process
    if (!p->in_memory || addr >= p->memory_allocation) {
        return;  // Invalid memory access, ignore write
    }

    // Write value to memory space
    memory_space[addr] = value;
}

// Read a uint16 value from memory for a given process
uint16_t read_from_memory(Process *p, uint16_t addr) {
    // Check if address is valid for this process
    if (!p->in_memory || addr >= p->memory_allocation) {
        return 0;  // Return 0 if memory not allocated or address out of bounds
    }

    // Return value from memory space (assuming it's initialized to 0 by default)
    return memory_space[addr];
}

typedef struct {
    bool occupied;
    int pid;
    int page_number;
    uint64_t last_used_tick;
} Frame;

static Frame *frame_table = NULL;
static int num_frames = 0;

extern uint64_t CPU_TICKS;
extern Process **process_table;
extern uint32_t num_processes;

void init_memory(uint64_t total_memory, uint64_t mem_per_frame, uint64_t max_mem_per_proc, uint64_t min_mem_per_proc) {
    memory.total_memory = total_memory;
    memory.mem_per_frame = mem_per_frame;
    memory.max_mem_per_proc = max_mem_per_proc;
    memory.min_mem_per_proc = min_mem_per_proc;
    memory.free_memory = total_memory;

    num_frames = total_memory / mem_per_frame;
    frame_table = calloc(num_frames, sizeof(Frame));
}


int is_valid_memory_address(uint32_t address) {
    return (address % 2 == 0) && (address < MAX_MEMORY_SIZE);
}

int handle_page_fault(Process *p, uint32_t virtual_address) {
    EnterCriticalSection(&backing_store_cs);  // Protect backing store access

    uint32_t offset = virtual_address - p->mem_base;
    uint32_t page_number = offset / memory.mem_per_frame;

    // Check for invalid page access
    if (page_number >= p->num_pages) {
        printf("[ACCESS VIOLATION] Invalid page access by P%d at 0x%X\n", p->pid, virtual_address);
        p->state = FINISHED;
        LeaveCriticalSection(&backing_store_cs);
        return 0;
    }

    // Try to find a free frame first
    for (int i = 0; i < num_frames; i++) {
        if (!frame_table[i].occupied) {
            // Load page from backing store into frame
            frame_table[i] = (Frame){true, p->pid, page_number, CPU_TICKS};
            p->page_table[page_number] = (PageTableEntry){i, true};
            
            // Read the page data from backing store
            uint32_t page_start = p->mem_base + (page_number * memory.mem_per_frame);
            uint32_t frame_start = i * memory.mem_per_frame / 2;  // Divide by 2 because memory_space is uint16_t array
            
            // Zero the frame contents first
            memset(&memory_space[frame_start], 0, memory.mem_per_frame);
            
            stats.num_paged_in++;
            LeaveCriticalSection(&backing_store_cs);
            return 1;
        }
    }

    // No free frames - need to select a victim using Enhanced LRU
    // printf("[Page Fault] No free frames available. Selecting victim frame...\n");
    
    int victim_idx = -1;
    uint64_t oldest_access = UINT64_MAX;
    Process *victim_process = NULL;

    // First try to find pages from finished or sleeping processes
    for (int i = 0; i < num_frames; i++) {
        if (!frame_table[i].occupied) continue;

        // Find the process that owns this frame
        Process *owner = NULL;
        for (int j = 0; j < num_processes; j++) {
            if (process_table[j] && process_table[j]->pid == frame_table[i].pid) {
                owner = process_table[j];
                break;
            }
        }

        // Prefer pages from finished/sleeping processes
        if (owner && (owner->state == FINISHED || owner->state == SLEEPING)) {
            victim_idx = i;
            victim_process = owner;
            break;
        }

        // Otherwise, track the least recently used frame
        if (frame_table[i].last_used_tick < oldest_access) {
            oldest_access = frame_table[i].last_used_tick;
            victim_idx = i;
            victim_process = owner;
        }
    }

    if (victim_idx == -1 || !victim_process) {
        printf("[ERROR] Failed to find a valid victim frame\n");
        LeaveCriticalSection(&backing_store_cs);
        return 0;
    }

    int victim_page = frame_table[victim_idx].page_number;
    printf("[Page Replacement] Evicting page %d of process %d from frame %d\n",
           victim_page, victim_process->pid, victim_idx);

    // Save the victim frame's contents to backing store
    uint32_t victim_frame_start = victim_idx * memory.mem_per_frame / 2;
    uint32_t victim_page_start = victim_process->mem_base + (victim_page * memory.mem_per_frame);

    // Copy frame contents to a temporary buffer
    uint16_t *page_buffer = malloc(memory.mem_per_frame);
    if (page_buffer) {
        memcpy(page_buffer, &memory_space[victim_frame_start], memory.mem_per_frame);
        
        // Mark the page as no longer in memory
        victim_process->page_table[victim_page].valid = false;
        
        // Write victim process state to backing store
        write_process_to_backing_store(victim_process);
        
        free(page_buffer);
        stats.num_paged_out++;
        
        printf("[Page Out] Successfully saved page %d of process %d to backing store\n",
               victim_page, victim_process->pid);
    } else {
        printf("[ERROR] Failed to allocate buffer for page out operation\n");
    }

    // Clear the frame and load the new page
    frame_table[victim_idx] = (Frame){true, p->pid, page_number, CPU_TICKS};
    p->page_table[page_number] = (PageTableEntry){victim_idx, true};

    // Zero the frame contents
    uint32_t frame_start = victim_idx * memory.mem_per_frame / 2;
    memset(&memory_space[frame_start], 0, memory.mem_per_frame);

    stats.num_paged_in++;
    LeaveCriticalSection(&backing_store_cs);
    return 1;
}


// int memory_write(uint32_t address, uint16_t value, Process *p) {
//    if (!is_valid_memory_address(address)) {
//        printf("[ACCESS VIOLATION] Process %d tried to write to 0x%X\n", p->pid, address);
//        return 0;
//    }
//    uint32_t offset = address - p->mem_base;
//    uint32_t page = offset / memory.mem_per_frame;
//    uint32_t page_offset = offset % memory.mem_per_frame;
//
//    if (page >= p->num_pages || !p->page_table[page].valid) {
//        if (!handle_page_fault(p, address)) return 0;
//    }
//
//    int physical_address = p->page_table[page].frame_number * memory.mem_per_frame + page_offset;
//    memory_space[physical_address / 2] = value;
//    frame_table[p->page_table[page].frame_number].last_used_tick = CPU_TICKS;
//    return 1;
//}

// uint16_t memory_read(uint32_t address, Process *p, int *success) {
//    if (!is_valid_memory_address(address)) {
//        printf("[ACCESS VIOLATION] Process %d tried to read from 0x%X\n", p->pid, address);
//        *success = 0;
//        return 0;
//    }
//    uint32_t offset = address - p->mem_base;
//    uint32_t page = offset / memory.mem_per_frame;
//    uint32_t page_offset = offset % memory.mem_per_frame;
//
//    if (page >= p->num_pages || !p->page_table[page].valid) {
//        if (!handle_page_fault(p, address)) {
//            *success = 0;
//            return 0;
//        }
//    }
//
//    int physical_address = p->page_table[page].frame_number * memory.mem_per_frame + page_offset;
//    frame_table[p->page_table[page].frame_number].last_used_tick = CPU_TICKS;
//    *success = 1;
//    return memory_space[physical_address / 2];
//}

// Recalculate free memory and active processes by walking the list
void update_free_memory() {
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

    memory.free_memory = free_mem;
    memory.num_processes_in_memory = proc_count;
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
void free_process_memory(Process *p, MemoryBlock **head_ref) {
    if(!p) return;
    MemoryBlock* curr = *head_ref;

    while (curr) {
        if (curr->occupied && curr->pid == p->pid) {
            curr->occupied = false;
            curr->pid = -1;
            p->in_memory = 0;
            break;
        }
        curr = curr->next;
    }

    // Merge adjacent free blocks to reduce fragmentation
    merge_adjacent_free_blocks(head_ref);

    // Recalculate memory stats
    update_free_memory();
    stats.num_paged_out++;
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

// void update_used_free_memory() {
//     // Allocate dynamic arrays for storing process info
//     int *temp_pids = malloc(sizeof(int) * num_processes);
//     uint64_t *temp_allocs = malloc(sizeof(uint64_t) * num_processes);
//     if (!temp_pids || !temp_allocs) {
//         fprintf(stderr, "Memory allocation failed\n");
//         free(temp_pids);
//         free(temp_allocs);
//         return;
//     }

//     int temp_count = 0;
//     uint64_t used_memory = 0;

//     // Collect memory allocations of active processes
//     for (int i = 0; i < num_processes; i++) {
//         Process *p = process_table[i];
//         if (p && (p->state == RUNNING || p->state == SLEEPING)) {
//             temp_pids[temp_count] = p->pid;
//             temp_allocs[temp_count] = p->memory_allocation;
//             used_memory += p->memory_allocation;
//             temp_count++;
//         }
//     }

//     memory.used_memory = used_memory;
//     memory.free_memory = memory.total_memory - used_memory;

// }
void update_used_free_memory() {
    uint64_t used_memory = 0;

    // Calculate used memory from actual memory blocks, not process table
    MemoryBlock* curr = memory_head;
    while (curr) {
        if (curr->occupied) {
            used_memory += (curr->end - curr->base + 1);
        }
        curr = curr->next;
    }
    
    memory.used_memory = used_memory;
    memory.free_memory = memory.total_memory - used_memory;
}

void process_smi(int num_cores, Process **cpu_cores) {

    // Allocate dynamic arrays for storing process info
    int *temp_pids = malloc(sizeof(int) * num_cores);  // Only need space for cores
    uint64_t *temp_allocs = malloc(sizeof(uint64_t) * num_cores);
    if (!temp_pids || !temp_allocs) {
        fprintf(stderr, "Memory allocation failed in process_smi()\n");
        free(temp_pids);
        free(temp_allocs);
        return;
    }

    int temp_count = 0;
    uint64_t used_memory = 0;

    // Only collect processes that are currently on CPU cores
    for (int i = 0; i < num_cores; i++) {
        Process *p = cpu_cores[i];
        if (p) {
            temp_pids[temp_count] = p->pid;
            temp_allocs[temp_count] = p->memory_allocation;
            used_memory += p->memory_allocation;
            temp_count++;
        }
    }

    double utilization = (num_cores > 0) ? (100.0 * temp_count / num_cores) : 0.0;

    // Output
    printf("----------------------------------------------\n");
    printf("| PROCESS-SMI V01.00 Driver Version: 01.00    |\n");
    printf("----------------------------------------------\n");
    printf("CPU-Util: %.2f%%\n", utilization);
    printf("Used Memory: %lldB\n", used_memory);
    printf("==============================================\n");
    printf("Running processes and memory usage:\n");
    printf("----------------------------------------------\n");

    for (int i = 0; i < temp_count; i++) {
        printf("P%d %lldB\n", temp_pids[i], temp_allocs[i]);
    }

    printf("----------------------------------------------\n");

    // Free dynamically allocated memory
    free(temp_pids);
    free(temp_allocs);
}


void vmstat(Memory *mem, CPUStats *stats) {
    printf("%10lld %4s %s\n", memory.total_memory, "B", "total memory");
    update_used_free_memory();
    printf("%10lld %4s %s\n", memory.used_memory, "B", "used memory");
    printf("%10lld %4s %s\n", memory.free_memory, "B", "free memory");
    printf("%10d %4s %s\n", stats->total_ticks, "B", "total ticks");
    printf("%10d %4s %s\n", stats->active_ticks, "", "active ticks");
    printf("%10d %4s %s\n", stats->idle_ticks, "", "idle ticks");
    printf("%10d %4s %s\n", stats->num_paged_in, "", "num paged in");
    printf("%10d %4s %s\n", stats->num_paged_out, "", "num paged out");
}
