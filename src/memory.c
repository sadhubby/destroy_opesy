#include "process.h"
#include "memory.h"
#include "scheduler.h"
#include "stats.h"
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
    // Ensure total memory doesn't exceed our static array size
    if (total_memory > MAX_MEMORY_SIZE) {
        printf("[ERROR] Total memory %llu exceeds maximum supported size %d\n", 
               total_memory, MAX_MEMORY_SIZE);
        total_memory = MAX_MEMORY_SIZE;
    }

    memory.total_memory = total_memory;
    memory.mem_per_frame = mem_per_frame;
    memory.max_mem_per_proc = max_mem_per_proc;
    memory.min_mem_per_proc = min_mem_per_proc;
    memory.num_frames = total_memory / mem_per_frame;

    // Calculate number of frames and ensure it doesn't exceed MAX_FRAMES
    num_frames = total_memory / mem_per_frame;
    if (num_frames > MAX_FRAMES) {
        printf("[ERROR] Calculated frames %d exceeds maximum frames %d\n", 
               num_frames, MAX_FRAMES);
        num_frames = MAX_FRAMES;
    }

    memory.num_frames = num_frames;

    frame_table = calloc(num_frames, sizeof(Frame));
    if (!frame_table) {
        printf("[FATAL] Failed to allocate frame table\n");
        exit(1);
    }

    // Initialize memory space to zero
    memset(memory_space, 0, sizeof(memory_space));
}


int is_valid_memory_address(uint32_t address) {
    return (address % 2 == 0) && (address < MAX_MEMORY_SIZE);
}

int handle_page_fault(Process *p, uint32_t virtual_address) {
    if (!p || !p->page_table) {
        printf("[ERROR] Invalid process or page table in page fault handler\n");
        return 0;
    }

    if (!frame_table) {
        printf("[ERROR] Frame table is NULL in page fault handler\n");
        return 0;
    }

    // Calculate page number from virtual address (which is already relative to process space)
    uint32_t page_number = virtual_address / memory.mem_per_frame;

    printf("[DEBUG-PAGEFAULT] Process %d: Page %u (VA=0x%X)\n", p->pid, page_number, virtual_address);

    if (page_number >= p->num_pages) {
        printf("[DEBUG] Virtual addr: 0x%X, Page: %d, Total pages: %d, Frame size: %lld\n", 
               virtual_address, page_number, p->num_pages, memory.mem_per_frame);
        printf("[ACCESS VIOLATION] Invalid page access by P%d at 0x%X\n", p->pid, virtual_address);
        p->state = FINISHED;
        return 0;
    }

    // First try to find a free frame
    for (int i = 0; i < num_frames; i++) {
        if (!frame_table[i].occupied) {
            // Clear the frame's memory
            uint32_t frame_start = i * memory.mem_per_frame;
            uint32_t frame_words = memory.mem_per_frame / 2;  // Divide by 2 since memory_space is uint16_t
            for (uint32_t j = 0; j < frame_words && (frame_start/2 + j) < MAX_MEMORY_SIZE/2; j++) {
                memory_space[frame_start/2 + j] = 0;
            }

            // Update frame table and page table
            frame_table[i] = (Frame){true, p->pid, page_number, CPU_TICKS};
            p->page_table[page_number] = (PageTableEntry){i, true};
            stats.num_paged_in++;

            printf("[DEBUG-PAGEFAULT] Allocated free frame %d for P%d page %u\n", 
                   i, p->pid, page_number);
            return 1;
        }
    }

    // No free frames, need to evict one
    // Find least recently used frame
    int victim_idx = 0;
    uint64_t oldest_tick = frame_table[0].last_used_tick;
    
    for (int i = 1; i < num_frames; i++) {
        if (frame_table[i].last_used_tick < oldest_tick) {
            oldest_tick = frame_table[i].last_used_tick;
            victim_idx = i;
        }
    }

    printf("[DEBUG-PAGEFAULT] Selected victim frame %d (P%d page %d) for P%d page %u\n",
           victim_idx, frame_table[victim_idx].pid, frame_table[victim_idx].page_number,
           p->pid, page_number);

    // Save victim info before modifying the frame
    int victim_pid = frame_table[victim_idx].pid;
    int victim_page = frame_table[victim_idx].page_number;

    // Find and update the victim process's page table
    bool victim_found = false;
    for (int i = 0; i < num_processes; i++) {
        Process *q = process_table[i];
        if (q && q->pid == victim_pid && q->page_table) {
            q->page_table[victim_page].valid = false;
            victim_found = true;
            printf("[DEBUG-PAGEFAULT] Marked page %d invalid for victim P%d\n",
                   victim_page, victim_pid);
            break;
        }
    }

    if (!victim_found) {
        printf("[WARNING] Could not find victim process P%d to invalidate page %d\n",
               victim_pid, victim_page);
    }

    // Validate frame index before using
    if (victim_idx >= memory.num_frames) {
        printf("[ERROR] Invalid victim frame index %d\n", victim_idx);
        return 0;
    }

    // Validate page number against process page table size
    if (page_number >= p->num_pages) {
        printf("[ERROR] Page number %d exceeds process P%d page table size %d\n",
               page_number, p->pid, p->num_pages);
        return 0;
    }

    // Clear the frame's memory
    uint32_t frame_start = victim_idx * memory.mem_per_frame;
    uint32_t frame_words = memory.mem_per_frame / 2;  // Divide by 2 since memory_space is uint16_t
    
    // Additional overflow check
    if (frame_start >= MAX_MEMORY_SIZE || frame_words > (MAX_MEMORY_SIZE - frame_start) / 2) {
        printf("[ERROR] Frame clearing would exceed memory bounds\n");
        return 0;
    }

    for (uint32_t j = 0; j < frame_words && (frame_start/2 + j) < MAX_MEMORY_SIZE/2; j++) {
        memory_space[frame_start/2 + j] = 0;
    }

    // Update frame table and page table with validation
    frame_table[victim_idx] = (Frame){true, p->pid, page_number, CPU_TICKS};
    p->page_table[page_number] = (PageTableEntry){victim_idx, true};

    // Verify the updates
    if (!frame_table[victim_idx].occupied || 
        frame_table[victim_idx].pid != p->pid || 
        frame_table[victim_idx].page_number != page_number) {
        printf("[ERROR] Frame table update verification failed\n");
        return 0;
    }

    if (!p->page_table[page_number].valid || 
        p->page_table[page_number].frame_number != victim_idx) {
        printf("[ERROR] Page table update verification failed\n");
        return 0;
    }

    stats.num_paged_in++;
    stats.num_paged_out++;

    printf("[DEBUG-PAGEFAULT] Successfully mapped P%d page %u to frame %d\n",
           p->pid, page_number, victim_idx);
    return 1;
}


int memory_write(uint32_t address, uint16_t value, Process *p) {
    if (!p) {
        printf("[ERROR] NULL process pointer passed to memory_write\n");
        return 0;
    }

    if (!is_valid_memory_address(address)) {
        printf("[ACCESS VIOLATION] Process %d tried to write to 0x%X\n", p->pid, address);
        return 0;
    }

    if (!p->page_table) {
        printf("[ERROR] Process %d has no page table\n", p->pid);
        return 0;
    }

    // In virtual memory, address is already relative to process space
    printf("[DEBUG-WRITE] Process %d: Virtual Address=0x%X\n", p->pid, address);
    uint32_t offset = address;  // Address is already relative to process space
    uint32_t page = offset / memory.mem_per_frame;
    uint32_t page_offset = offset % memory.mem_per_frame;

    if (page >= p->num_pages || !p->page_table[page].valid) {
        if (!handle_page_fault(p, address)) return 0;
    }

    int physical_address = p->page_table[page].frame_number * memory.mem_per_frame + page_offset;
    memory_space[physical_address / 2] = value;
    frame_table[p->page_table[page].frame_number].last_used_tick = CPU_TICKS;
    return 1;
}

uint16_t memory_read(uint32_t address, Process *p, int *success) {
    if (!p || !success) {
        printf("[ERROR] NULL pointer passed to memory_read\n");
        if (success) *success = 0;
        return 0;
    }

    if (!is_valid_memory_address(address)) {
        printf("[ACCESS VIOLATION] Process %d tried to read from 0x%X\n", p->pid, address);
        *success = 0;
        return 0;
    }

    if (!p->page_table) {
        printf("[ERROR] Process %d has no page table\n", p->pid);
        *success = 0;
        return 0;
    }

    // In a virtual memory system, all addresses from processes are virtual (relative to 0)
    // No need to check against mem_base since addresses are already virtual
    // Sanity check the process state first
    if (p->num_pages == 0 || p->memory_allocation == 0) {
        printf("[ERROR] Process %d has invalid memory state: num_pages=%u, memory_allocation=%u\n",
               p->pid, p->num_pages, p->memory_allocation);
        *success = 0;
        return 0;
    }

    printf("[DEBUG-READ] Process %d: Virtual Address=0x%X (allocation=%u, num_pages=%u)\n", 
           p->pid, address, p->memory_allocation, p->num_pages);
    uint32_t offset = address;  // Address is already relative to process space
    uint32_t page = offset / memory.mem_per_frame;
    uint32_t page_offset = offset % memory.mem_per_frame;

    printf("[DEBUG-READ] Calculated: offset=%u, page=%u, page_offset=%u, num_pages=%u\n", 
           offset, page, page_offset, p->num_pages);

    // Check page number bounds
    if (page >= p->num_pages) {
        printf("[DEBUG-READ] Page number %u exceeds process page table size %u\n", page, p->num_pages);
        *success = 0;
        return 0;
    }

    if (!p->page_table) {
        printf("[DEBUG-READ] Page table is NULL for process %d\n", p->pid);
        *success = 0;
        return 0;
    }

    if (!p->page_table[page].valid) {
        printf("[DEBUG-READ] Page fault on page %u, calling handler\n", page);
        if (!handle_page_fault(p, address)) {
            *success = 0;
            return 0;
        }
        
        // Recheck page validity after page fault handling
        if (!p->page_table[page].valid) {
            printf("[ERROR] Page %u still invalid after page fault handling\n", page);
            *success = 0;
            return 0;
        }
    }

    // Verify page_table and frame_table are valid before accessing
    if (!frame_table) {
        printf("[ERROR] Frame table is NULL\n");
        *success = 0;
        return 0;
    }

    // Save frame number to local var to prevent multiple dereferences
    int frame_number = p->page_table[page].frame_number;
    if (frame_number < 0 || frame_number >= num_frames) {
        printf("[ERROR] Invalid frame number %d (max: %d) for P%d page %u\n", 
               frame_number, num_frames - 1, p->pid, page);
        *success = 0;
        return 0;
    }

    // Check that the frame is actually allocated to this process
    if (!frame_table[frame_number].occupied || frame_table[frame_number].pid != p->pid) {
        printf("[ERROR] Frame %d is not allocated to P%d\n", frame_number, p->pid);
        *success = 0;
        return 0;
    }

    // Double check memory configuration
    if (memory.mem_per_frame == 0) {
        printf("[ERROR] Invalid frame size of 0\n");
        *success = 0;
        return 0;
    }

    // Validate page offset
    if (page_offset >= memory.mem_per_frame) {
        printf("[ERROR] Page offset %u exceeds frame size %llu\n", 
               page_offset, memory.mem_per_frame);
        *success = 0;
        return 0;
    }

    // Calculate physical address with overflow checking
    uint64_t temp_addr = (uint64_t)frame_number * memory.mem_per_frame + page_offset;
    if (temp_addr >= MAX_MEMORY_SIZE) {
        printf("[ERROR] Physical address calculation overflow: %llu exceeds %d\n", 
               temp_addr, MAX_MEMORY_SIZE - 1);
        *success = 0;
        return 0;
    }

    // Validate frame alignment
    if (temp_addr % 2 != 0) {
        printf("[ERROR] Calculated physical address %llu is not word-aligned\n", temp_addr);
        *success = 0;
        return 0;
    }

    int physical_address = (int)temp_addr;

    // Double check the physical address is within bounds of our memory array
    if (physical_address / 2 >= MAX_MEMORY_SIZE / 2) {
        printf("[ERROR] Physical address %d out of bounds for memory array size %d\n", 
               physical_address, MAX_MEMORY_SIZE);
        *success = 0;
        return 0;
    }

    // Update access time and return memory contents
    frame_table[frame_number].last_used_tick = CPU_TICKS;
    *success = 1;
    return memory_space[physical_address / 2];
}

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
