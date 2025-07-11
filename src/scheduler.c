#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include "scheduler.h"
#include "process.h"
#include "config.h"

uint64_t CPU_TICKS = 0;
volatile int scheduler_running = 0;
HANDLE scheduler_thread;
ReadyQueue ready_queue;
Process **cpu_cores = NULL;
int num_cores = 0;
Config config;

static uint64_t last_process_tick = 0;
CRITICAL_SECTION cpu_cores_cs;
HANDLE *core_threads = NULL;

// initialize the ready queue
void init_ready_queue() {
    ready_queue.capacity = 16;
    ready_queue.size = 0;
    ready_queue.head = 0;
    ready_queue.tail = 0;
    ready_queue.items = malloc(sizeof(Process *) * ready_queue.capacity);
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

// fcfs scheduling
void schedule_fcfs() {

    // assign ready processes to free CPUs
    EnterCriticalSection(&cpu_cores_cs);
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] == NULL || cpu_cores[i]->state == FINISHED) {
            Process *next = dequeue_ready();
            if (next) {
                cpu_cores[i] = next;
                if (next->state == READY) {
                    next->state = RUNNING;
                }
            }
        }
    }
    LeaveCriticalSection(&cpu_cores_cs);

    // execute running processes
    for (int i = 0; i < num_cores; i++) {
        Process *p = cpu_cores[i];
        if (p && p->state == RUNNING) {
            execute_instruction(p);
            // mark finished
            if (p->program_counter >= p->num_inst && p->for_depth == 0) {
                p->state = FINISHED;
                cpu_cores[i] = NULL;
            }
        }
    }
}

// main scheduler loop
DWORD WINAPI scheduler_loop(LPVOID lpParam) {
    while (scheduler_running) {
        CPU_TICKS++;

        // Generate a new process
        if (config.batch_process_freq > 0 && (CPU_TICKS - last_process_tick) >= (uint64_t)config.batch_process_freq) {
            Process *dummy = generate_dummy_process(config);
            add_process(dummy);
            enqueue_ready(dummy);
            last_process_tick = CPU_TICKS;
        }

        // wake up sleeping processes
        for (int i = 0; i < num_processes; i++) {
            Process *p = (process_table[i]);
            if (p->state == SLEEPING && CPU_TICKS >= p->sleep_until_tick) {
                p->state = READY;
            }
        }

        // fcfs first
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

        if (p && p->state == RUNNING) {
            LeaveCriticalSection(&cpu_cores_cs);
            execute_instruction(p);
            EnterCriticalSection(&cpu_cores_cs);

            if (p->program_counter >= p->num_inst && p->for_depth == 0) {
                p->state = FINISHED;
                cpu_cores[core_id] = NULL;
            }
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
    scheduler_thread = CreateThread(NULL, 0, scheduler_loop, NULL, 0, NULL);
    start_core_threads();
}

// close scheduler thread
void stop_scheduler() {
    scheduler_running = 0;
    WaitForSingleObject(scheduler_thread, INFINITE);
    CloseHandle(scheduler_thread);
    stop_core_threads();
}

void busy_wait_ticks(uint32_t delay_ticks) {
    uint64_t target_tick = CPU_TICKS + delay_ticks;
    while (CPU_TICKS < target_tick) {
        // to add more code here
    }
}

// create core array
void init_cpu_cores(int n) {
    num_cores = n;
    cpu_cores = malloc(sizeof(Process *) * n);
    for (int i = 0; i < n; i++) cpu_cores[i] = NULL;
}

void start_core_threads() {
    InitializeCriticalSection(&cpu_cores_cs);
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