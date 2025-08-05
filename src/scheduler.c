#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "scheduler.h"
#include "process.h"
#include "config.h"
#include "memory.h"
#include "stats.h"
#include "backing_store.h"

uint64_t CPU_TICKS = 0;
uint64_t switch_tick = 0;
volatile int scheduler_running = 0;
volatile int processes_generating = 0;
HANDLE scheduler_thread;
ReadyQueue ready_queue;
Process **cpu_cores = NULL;
int num_cores = 0;
int quantum;
Config config ;

static uint64_t last_process_tick = 0;
CRITICAL_SECTION cpu_cores_cs;
CRITICAL_SECTION backing_store_cs;
HANDLE *core_threads = NULL;
static int quantum_cycle = 0; //new add
// 0 is fcfs, 1 is rr
int schedule_type = 0;

// finished process array
static Process **finished_processes = NULL;
static int finished_count = 0;
static int finished_capacity = 0;
bool try_allocate_memory(Process* process, MemoryBlock* memory_blocks_head);
double utilization = 0.0;
int used = 0;

void update_cpu_util(int add) {
    used += add;
    utilization = (num_cores > 0) ? (100.0 * used / num_cores) : 0.0;
}

void add_finished_process(Process *p) {
    /* printf("[DEBUG] Finishing process: %s (PID: %d)\n", p->name, p->pid);
 */
    if (finished_count == finished_capacity) {
        int new_cap = finished_capacity == 0 ? 16 : finished_capacity * 2;
        Process **new_arr = malloc(sizeof(Process *) * new_cap);
        for (int i = 0; i < finished_count; i++) new_arr[i] = finished_processes[i];
        free(finished_processes);
        finished_processes = new_arr;
        finished_capacity = new_cap;
    }
    finished_processes[finished_count++] = p;
}

// initialize the ready queue
void init_ready_queue() {
    ready_queue.capacity = 16;
    ready_queue.size = 0;
    ready_queue.head = 0;
    ready_queue.tail = 0;
    ready_queue.items = malloc(sizeof(Process *) * ready_queue.capacity);
    // finished process array
    finished_capacity = 16;
    finished_count = 0;
    finished_processes = malloc(sizeof(Process *) * finished_capacity);
}

void print_ready_queue() {
    printf("\n[READY QUEUE]\n");
    printf("%-8s %-8s %-8s\n", "Index", "PID", "Name");
    for (uint32_t i = 0; i < ready_queue.size; i++) {
        uint32_t idx = (ready_queue.head + i) % ready_queue.capacity;
        Process *p = ready_queue.items[idx];
        if (p) {
            printf("%-8u %-8d %-8s\n", i, p->pid, p->name);
        }
    }
    printf("[END READY QUEUE]\n");
}

// eunqueue new process
void enqueue_ready(Process *p) {
    // resize the ready queue if too many items
    if (ready_queue.size == ready_queue.capacity) {
        uint32_t new_cap = ready_queue.capacity * 2;
        Process **new_items = malloc(sizeof(Process *) * new_cap);

        // add to new items
        for (uint32_t i = 0; i < ready_queue.size; i++) {
            new_items[i] = ready_queue.items[(ready_queue.head + i) % ready_queue.capacity];
        }

        // reset
        free(ready_queue.items);
        ready_queue.items = new_items;
        ready_queue.capacity = new_cap;
        ready_queue.head = 0;
        ready_queue.tail = ready_queue.size;
    }

    ready_queue.items[ready_queue.tail] = p;
    ready_queue.tail = (ready_queue.tail + 1) % ready_queue.capacity;
    ready_queue.size++;
}

// dequeue, returns the head
Process *dequeue_ready() {
    if (ready_queue.size == 0) return NULL;

    Process *p = ready_queue.items[ready_queue.head];
    ready_queue.head = (ready_queue.head + 1) % ready_queue.capacity;
    ready_queue.size--;
    return p;
}
void handle_memory_pressure() {
    extern bool is_memory_under_pressure();
    extern Process* find_victim_process_for_swapping();
    extern MemoryBlock* memory_head;
    
    if (!is_memory_under_pressure()) return;
    
    Process *victim = find_victim_process_for_swapping();
    if (!victim) return;
    
    printf("[SWAP] Memory pressure detected. Swapping out process %s (PID: %d)\n", 
           victim->name, victim->pid);
    
    // Write to backing store first
    write_process_to_backing_store(victim);
    
    // Remove from CPU if currently running
    EnterCriticalSection(&cpu_cores_cs);
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] == victim) {
            cpu_cores[i] = NULL;
            update_cpu_util(-1);
            break;
        }
    }
    LeaveCriticalSection(&cpu_cores_cs);
    
    // Free its memory
    free_process_memory(victim, &memory_head);
    victim->in_memory = 0;
    
    // Remove from ready queue if present
    // Note: This is a simplified approach - you might want to implement
    // a more sophisticated ready queue removal function
    
    printf("[SWAP] Process %s successfully swapped to backing store\n", victim->name);
}
// fcfs scheduling
void schedule_fcfs() {
   EnterCriticalSection(&cpu_cores_cs);

    // Wake up sleeping processes
    for (int i = 0; i < num_cores; i++) {
        Process *p = cpu_cores[i];
        if (p && p->state == SLEEPING && CPU_TICKS >= p->sleep_until_tick) {
            p->state = RUNNING;
        }
    }

    // 1. Assign ready processes to free cores
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] == NULL) {
            Process *next = dequeue_ready();
            if (!next) continue;

            // Validate process data before scheduling
            if (next->in_memory == 1 || try_allocate_memory(next, memory_head)) {
                // Extra validation to prevent crashes
                if (next->instructions != NULL && next->variables != NULL) {
                    update_cpu_util(1);
                    cpu_cores[i] = next;
                    next->core = i;
                    next->state = RUNNING;
                    next->last_exec_time = time(NULL);
                    
                    if (next->in_memory == 0) {
                        next->in_memory = 1;
                        update_free_memory();
                    }
                } else {
                    // Don't schedule this process
                    if (next->instructions) free(next->instructions);
                    if (next->variables) free(next->variables);
                    free(next);
                }
            } else {
                // Can't allocate memory - send to backing store
                write_process_to_backing_store(next);
                free(next);
            }
        }
    }

    // 3. Try to swap in processes from backing store (periodically)
    if (CPU_TICKS > 0 && CPU_TICKS % 50 == 0) {
        Process *swapped_in = read_first_process_from_backing_store();
        if (swapped_in) {
            // Validate the process from backing store
            if (swapped_in->num_inst <= 0 || swapped_in->num_inst > 1000000) {
                free(swapped_in);
            } else if (try_allocate_memory(swapped_in, memory_head)) {
                // Successfully allocated memory
                remove_first_process_from_backing_store();
                swapped_in->in_memory = 1;
                enqueue_ready(swapped_in);
                update_free_memory();
            } else {
                // Memory full - find a victim to swap out
                Process *victim = NULL;
                int victim_core = -1;
                
                for (int j = 0; j < num_cores; j++) {
                    if (cpu_cores[j] && cpu_cores[j]->state == RUNNING) {
                        victim = cpu_cores[j];
                        victim_core = j;
                        break;
                    }
                }
                
                if (victim) {
                    // Only write to backing store if we have valid data
                    if (victim->instructions && victim->variables) {
                        write_process_to_backing_store(victim);
                    }
                    
                    free_process_memory(victim, &memory_head);
                    cpu_cores[victim_core] = NULL;
                    update_cpu_util(-1);
                    
                    if (victim->instructions) free(victim->instructions);
                    if (victim->variables) free(victim->variables);
                    free(victim);
                    
                    // Try again with the new free memory
                    if (try_allocate_memory(swapped_in, memory_head)) {
                        remove_first_process_from_backing_store();
                        swapped_in->in_memory = 1;
                        enqueue_ready(swapped_in);
                        update_free_memory();
                    } else {
                        if (swapped_in->instructions) free(swapped_in->instructions);
                        if (swapped_in->variables) free(swapped_in->variables);
                        free(swapped_in);
                    }
                } else {
                    // No victim found, free the swapped-in process
                    if (swapped_in->instructions) free(swapped_in->instructions);
                    if (swapped_in->variables) free(swapped_in->variables);
                    free(swapped_in);
                }
            }
        }
    }

    // 4. If all cores are idle and ready queue is empty, try to swap in from backing store
    bool all_idle = true;
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] && cpu_cores[i]->state == RUNNING) {
            all_idle = false;
            break;
        }
    }
    
    if (all_idle && ready_queue.size == 0) {
        Process *swapped_in = read_first_process_from_backing_store();
        if (swapped_in) {
            // Validate the process
            if (swapped_in->num_inst <= 0 || swapped_in->num_inst > 1000000) {
                free(swapped_in);
            } else if (try_allocate_memory(swapped_in, memory_head)) {
                // Success - remove from backing store and add to ready queue
                remove_first_process_from_backing_store();
                swapped_in->in_memory = 1;
                enqueue_ready(swapped_in);
                update_free_memory();
            } else {
                // Failed to allocate memory
                if (swapped_in->instructions) free(swapped_in->instructions);
                if (swapped_in->variables) free(swapped_in->variables);
                free(swapped_in);
            }
        }
    }

    LeaveCriticalSection(&cpu_cores_cs);
}

void schedule_rr() {
     EnterCriticalSection(&cpu_cores_cs);

    // Wake up sleeping processes
    for (int i = 0; i < num_cores; i++) {
        Process *p = cpu_cores[i];
        if (p && p->state == SLEEPING && CPU_TICKS >= p->sleep_until_tick) {
            p->state = RUNNING;
            p->ticks_ran_in_quantum = 0;
        }
    }

    // 1. Preempt processes that have used up their quantum
    for (int i = 0; i < num_cores; i++) {
        Process *p = cpu_cores[i];
        if (p && p->state == RUNNING && p->ticks_ran_in_quantum >= quantum) {
            update_cpu_util(-1);
            cpu_cores[i] = NULL;
            p->state = READY;
            enqueue_ready(p);
        }
    }

    // 2. Assign ready processes to free cores
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] == NULL) {
            Process *next = dequeue_ready();
            if (!next) continue;

            // Validate process data before scheduling
            if (next->in_memory == 1 || try_allocate_memory(next, memory_head)) {
                // Extra validation to prevent crashes
                if (next->instructions != NULL && next->variables != NULL) {
                    update_cpu_util(1);
                    cpu_cores[i] = next;
                    next->core = i;
                    next->state = RUNNING;
                    next->last_exec_time = time(NULL);
                    
                    if (next->in_memory == 0) {
                        next->in_memory = 1;
                        update_free_memory();
                    }
                    next->ticks_ran_in_quantum = 0;
                } else {
                    // Don't schedule this process
                    if (next->instructions) free(next->instructions);
                    if (next->variables) free(next->variables);
                    free(next);
                }
            } else {
                // Can't allocate memory - send to backing store
                write_process_to_backing_store(next);
                free(next);
            }
        }
    }

    // 3. Try to swap in processes from backing store (periodically)
    if (CPU_TICKS > 0 && CPU_TICKS % 50 == 0) {
        Process *swapped_in = read_first_process_from_backing_store();
        if (swapped_in) {
            // Validate the process from backing store
            if (swapped_in->num_inst <= 0 || swapped_in->num_inst > 1000000) {
                free(swapped_in);
            } else if (try_allocate_memory(swapped_in, memory_head)) {
                // Successfully allocated memory
                remove_first_process_from_backing_store();
                swapped_in->in_memory = 1;
                enqueue_ready(swapped_in);
                update_free_memory();
            } else {
                // Memory full - find a victim to swap out
                Process *victim = NULL;
                int victim_core = -1;
                
                for (int j = 0; j < num_cores; j++) {
                    if (cpu_cores[j] && cpu_cores[j]->state == RUNNING) {
                        victim = cpu_cores[j];
                        victim_core = j;
                        break;
                    }
                }
                
                if (victim) {
                    // Only write to backing store if we have valid data
                    if (victim->instructions && victim->variables) {
                        write_process_to_backing_store(victim);
                    }
                    
                    free_process_memory(victim, &memory_head);
                    cpu_cores[victim_core] = NULL;
                    update_cpu_util(-1);
                    
                    if (victim->instructions) free(victim->instructions);
                    if (victim->variables) free(victim->variables);
                    free(victim);
                    
                    // Try again with the new free memory
                    if (try_allocate_memory(swapped_in, memory_head)) {
                        remove_first_process_from_backing_store();
                        swapped_in->in_memory = 1;
                        enqueue_ready(swapped_in);
                        update_free_memory();
                    } else {
                        if (swapped_in->instructions) free(swapped_in->instructions);
                        if (swapped_in->variables) free(swapped_in->variables);
                        free(swapped_in);
                    }
                } else {
                    // No victim found, free the swapped-in process
                    if (swapped_in->instructions) free(swapped_in->instructions);
                    if (swapped_in->variables) free(swapped_in->variables);
                    free(swapped_in);
                }
            }
        }
    }

    // 4. If all cores are idle and ready queue is empty, try to swap in from backing store
    bool all_idle = true;
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] && cpu_cores[i]->state == RUNNING) {
            all_idle = false;
            break;
        }
    }
    
    if (all_idle && ready_queue.size == 0) {
        Process *swapped_in = read_first_process_from_backing_store();
        if (swapped_in) {
            // Validate the process
            if (swapped_in->num_inst <= 0 || swapped_in->num_inst > 1000000) {
                free(swapped_in);
            } else if (try_allocate_memory(swapped_in, memory_head)) {
                // Success - remove from backing store and add to ready queue
                remove_first_process_from_backing_store();
                swapped_in->in_memory = 1;
                enqueue_ready(swapped_in);
                update_free_memory();
            } else {
                // Failed to allocate memory
                if (swapped_in->instructions) free(swapped_in->instructions);
                if (swapped_in->variables) free(swapped_in->variables);
                free(swapped_in);
            }
        }
    }

    LeaveCriticalSection(&cpu_cores_cs);
}


// main scheduler loop
DWORD WINAPI scheduler_loop(LPVOID lpParam) {
   while (scheduler_running) {
        CPU_TICKS++;
        Sleep(1);

        // Generate a new process when needed
        if (processes_generating) {
            if (config.batch_process_freq > 0 && (CPU_TICKS - last_process_tick) >= (uint64_t)config.batch_process_freq) {
                Process *dummy = generate_dummy_process(config);
                if (dummy) {
                    // *** FIX: Try to allocate memory BEFORE adding to process table ***
                    if (memory_head && try_allocate_memory(dummy, memory_head)) {
                        // Memory allocation successful - NOW add to process table and ready queue
                        add_process(dummy);
                        dummy->in_memory = 1;
                        enqueue_ready(dummy);
                        update_free_memory();
                    } else {
                        // *** FIX: Memory allocation failed - send directly to backing store ***
                        write_process_to_backing_store(dummy);
                        
                        // *** CRITICAL FIX: Free the process after writing to backing store ***
                        if (dummy->instructions) free(dummy->instructions);
                        if (dummy->variables) free(dummy->variables);
                        if (dummy->page_table) free(dummy->page_table);
                        free(dummy);
                    }
                }
                last_process_tick = CPU_TICKS;
            }
        }

        if (schedule_type)
            schedule_rr();
        else
            schedule_fcfs();
    }
    return 0;
}

// Per-core thread function
DWORD WINAPI core_loop(LPVOID lpParam) {
     int core_id = (int)(intptr_t)lpParam;

    while (scheduler_running) {
        EnterCriticalSection(&cpu_cores_cs);
        Process *p = cpu_cores[core_id];
        int should_execute = (p && p->state == RUNNING);
        LeaveCriticalSection(&cpu_cores_cs);

        if (should_execute) {
            if (p && p->program_counter < p->num_inst && 
                p->instructions != NULL && p->variables != NULL) {
                
                Instruction *inst = &p->instructions[p->program_counter];
                if (inst) {
                    execute_instruction(p, config);
                    p->ticks_ran_in_quantum++;
                }
            } else {
                // Silently handle invalid process - NO PRINTS!
                EnterCriticalSection(&cpu_cores_cs);
                if (p) {
                    cpu_cores[core_id] = NULL;
                    update_cpu_util(-1);
                }
                LeaveCriticalSection(&cpu_cores_cs);
            }
        }

        // Handle process completion
        EnterCriticalSection(&cpu_cores_cs);
        p = cpu_cores[core_id];
        if (p && p->program_counter >= p->num_inst && p->for_depth == 0) {
            p->state = FINISHED;
            cleanup_process(p);
            add_finished_process(p);
            
            free_process_memory(p, &memory_head);
            update_free_memory();
            update_cpu_util(-1);
            
            cpu_cores[core_id] = NULL;
            p = NULL;
        }
        LeaveCriticalSection(&cpu_cores_cs);

        Sleep(1);
    }
    return 0;
}

// scheduler thread create
void start_scheduler(Config system_config) {
    config = system_config;
    scheduler_running = 1;
    processes_generating = 1;
    quantum = config.quantum_cycles;
    init_memory(config.max_overall_mem, config.mem_per_frame, config.max_mem_per_proc, config.min_mem_per_proc);
    init_stats();
    memory_head = init_memory_block(config.max_overall_mem);
    if (strcmp(config.scheduler, "rr") == 0)
        schedule_type = 1;

    scheduler_thread = CreateThread(NULL, 0, scheduler_loop, NULL, 0, NULL);
    start_core_threads();
}

void start_scheduler_without_processes(Config system_config) {
    config = system_config;
    scheduler_running = 1;
    processes_generating = 0;
    quantum = config.quantum_cycles;
    init_memory(config.max_overall_mem, config.mem_per_frame, config.max_mem_per_proc, config.min_mem_per_proc);
    init_stats();
    memory_head = init_memory_block(config.max_overall_mem);
    if (strcmp(config.scheduler, "rr") == 0)
        schedule_type = 1;

    scheduler_thread = CreateThread(NULL, 0, scheduler_loop, NULL, 0, NULL);
    start_core_threads();
}

void stop_scheduler() {
    processes_generating = 0;
}

void busy_wait_ticks(uint32_t delay_ticks) {
    uint64_t target_tick = CPU_TICKS + delay_ticks;

    while (CPU_TICKS < target_tick) {
        _ReadWriteBarrier();
        Sleep(0);
    }
}

// create core array
void init_cpu_cores(int n) {
    num_cores = n;
    cpu_cores = malloc(sizeof(Process *) * n);
    used = 0;
    utilization = 0.0;
    for (int i = 0; i < n; i++) {
        cpu_cores[i] = NULL;
    }
}

void start_core_threads() {
    InitializeCriticalSection(&cpu_cores_cs);
    InitializeCriticalSection(&backing_store_cs);
    core_threads = malloc(sizeof(HANDLE) * num_cores);

    for (int i = 0; i < num_cores; i++)
        core_threads[i] = CreateThread(NULL, 0, core_loop, (LPVOID)(intptr_t)i, 0, NULL);
}

void stop_core_threads() {

    for (int i = 0; i < num_cores; i++) {
        WaitForSingleObject(core_threads[i], INFINITE);
        CloseHandle(core_threads[i]);
    }
    
    free(core_threads);
    DeleteCriticalSection(&cpu_cores_cs);
}

int get_num_cores() {
    return num_cores;
}

Process **get_cpu_cores() {
    return cpu_cores;
}

Process **get_finished_processes() {
    return finished_processes;
}

int get_finished_count() {
    return finished_count;
}

bool try_allocate_memory(Process* process, MemoryBlock* memory_blocks_head) {
     if (!process || !memory_blocks_head) {
        // printf("[MEMORY] Invalid parameters to try_allocate_memory\n");
        return false;
    }

    // *** FIX: Add validation for process memory allocation ***
    if (process->memory_allocation == 0) {
        // printf("[MEMORY] Process P%d has zero memory allocation\n", process->pid);
        return false;
    }

    MemoryBlock* curr = memory_blocks_head;

    while (curr != NULL) {
        uint64_t block_size = curr->end - curr->base + 1;

        // *** FIX: Check if block is NOT occupied AND big enough ***
        if (!curr->occupied && block_size >= process->memory_allocation) {
            // Set process memory bounds
            process->mem_base = curr->base;
            process->mem_limit = curr->base + process->memory_allocation - 1;
            
            // *** CRITICAL FIX: Mark the block as occupied FIRST ***
            curr->occupied = true;
            curr->pid = process->pid;

            // *** FIX: Split the block if there's leftover space ***
            if (block_size > process->memory_allocation) {
                MemoryBlock* new_block = malloc(sizeof(MemoryBlock));
                if (new_block) {
                    // New block starts after allocated space
                    new_block->base = curr->base + process->memory_allocation;
                    new_block->end = curr->end;
                    new_block->occupied = false;
                    new_block->pid = -1;
                    new_block->next = curr->next;

                    // *** CRITICAL: Update the current block's end to only cover allocated space ***
                    curr->end = curr->base + process->memory_allocation - 1;
                    curr->next = new_block;
                } else {
                    // printf("[MEMORY] Warning: Failed to create split block - continuing without split\n");
                }
            }

            stats.num_paged_in++;
            return true;
        }

        curr = curr->next;
    }

    return false;
}
