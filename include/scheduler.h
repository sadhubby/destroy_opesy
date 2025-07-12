#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <windows.h>
#include "process.h"
#include "config.h"
#include "memory.h"

typedef struct ReadyQueue {
    Process **items;
    uint32_t capacity;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
} ReadyQueue;

extern Process **cpu_cores;
extern uint64_t CPU_TICKS;
extern int num_cores;
// FIFO for processes
extern ReadyQueue ready_queue;
extern MemoryBlock **memory_blocks;

DWORD WINAPI scheduler_loop(LPVOID lpParam);
void start_scheduler(Config config);
void stop_scheduler();
void busy_wait_ticks(uint32_t delay_ticks);

void init_ready_queue();
void enqueue_ready(Process *p);
Process *dequeue_ready();

void assign_processes_to_cores();
void scheduler_tick();

void init_cpu_cores(int n);
void start_core_threads();
void stop_core_threads();

int get_num_cores();
Process **get_cpu_cores();
Process **get_finished_processes();
int get_finished_count();

#endif