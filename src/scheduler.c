#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "scheduler.h"
#include "process.h"
#include "config.h"

uint64_t CPU_TICKS = 0;
uint64_t switch_tick = 0;
volatile int scheduler_running = 0;
volatile int processes_generating = 0;
HANDLE scheduler_thread;
ReadyQueue ready_queue;
Process **cpu_cores = NULL;
int num_cores = 0;
int quantum;
Config config;

static uint64_t last_process_tick = 0;
CRITICAL_SECTION cpu_cores_cs;
HANDLE *core_threads = NULL;

// 0 is fcfs, 1 is rr
int schedule_type = 0;

// finished process array
static Process **finished_processes = NULL;
static int finished_count = 0;
static int finished_capacity = 0;

void add_finished_process(Process *p) {
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
        // If the core has no process, or the process is FINISHED, set to NULL
        if (cpu_cores[i] == NULL || (cpu_cores[i] && cpu_cores[i]->state == FINISHED)) {
            cpu_cores[i] = NULL;
        }

        // Only assign to free core
        if (cpu_cores[i] == NULL) {
            Process *next = dequeue_ready();
            if (next) {
                cpu_cores[i] = next;
                if (next->state == READY) {
                    next->state = RUNNING;
                }
            }
        }
    }

    // Wake up sleeping processes before executing instructions
    for (int i = 0; i < num_cores; i++) {
        Process *p = cpu_cores[i];
        if (p && p->state == SLEEPING && CPU_TICKS >= p->sleep_until_tick) {
            p->state = RUNNING;
        }
    }

    LeaveCriticalSection(&cpu_cores_cs);
}

void schedule_rr () {

    // assign ready processes to free CPUs
    
    EnterCriticalSection(&cpu_cores_cs);

    for (int i = 0; i < num_cores; i++) {
        // If the core has no process, or the process is FINISHED, set to NULL
        if (cpu_cores[i] && cpu_cores[i]->state == FINISHED) {
            cpu_cores[i] = NULL;
        }

        // Only assign to free core

        if (cpu_cores[i] == NULL) {
            Process *next = dequeue_ready();

            if (next) {
                cpu_cores[i] = next;
                switch_tick = CPU_TICKS + quantum;
                if (next->state == READY) {
                    next->state = RUNNING;
                }
            }
        }
    }

    // Wake up sleeping processes before executing instructions
    for (int i = 0; i < num_cores; i++) {
        Process *p = cpu_cores[i];
        if (p && p->state == SLEEPING && CPU_TICKS >= p->sleep_until_tick) {
            p->state = RUNNING;
        }

        // put back to ready queue
        if (p && p->state == RUNNING && CPU_TICKS >= switch_tick) {
            cpu_cores[i] = NULL;
            p->state = READY;
            enqueue_ready(p);
        }
    }

    LeaveCriticalSection(&cpu_cores_cs);
}

// main scheduler loop
DWORD WINAPI scheduler_loop(LPVOID lpParam) {
    while (scheduler_running) {
        CPU_TICKS++;
        Sleep(1);

        // Generate a new process
        if (processes_generating) {
            if (config.batch_process_freq > 0 && (CPU_TICKS - last_process_tick) >= (uint64_t)config.batch_process_freq) {
                Process *dummy = generate_dummy_process(config);
                add_process(dummy);
                enqueue_ready(dummy);
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

        // Only print and fetch state while holding the lock
        int should_execute = (p && p->state == RUNNING);
        LeaveCriticalSection(&cpu_cores_cs);


        if (should_execute) {
            execute_instruction(p, config);
            
            EnterCriticalSection(&cpu_cores_cs);
            LeaveCriticalSection(&cpu_cores_cs);
            if (config.delay_per_exec > 0) {
                busy_wait_ticks(config.delay_per_exec);
            }
        }

        EnterCriticalSection(&cpu_cores_cs);
        p = cpu_cores[core_id];
        if (p && p->program_counter >= p->num_inst && p->for_depth == 0) {
            p->state = FINISHED;
            add_finished_process(p);
            cpu_cores[core_id] = NULL;
            p = NULL;
        }

        LeaveCriticalSection(&cpu_cores_cs);
    }
    return 0;
}

// scheduler thread create
void start_scheduler(Config system_config) {
    config = system_config;
    scheduler_running = 1;
    processes_generating = 1;
    quantum = config.quantum_cycles;

    if (strcmp(config.scheduler, "rr") == 0)
        schedule_type = 1;

    scheduler_thread = CreateThread(NULL, 0, scheduler_loop, NULL, 0, NULL);
    start_core_threads();
}

void stop_scheduler() {
    processes_generating = 0;
    printf("%d", processes_generating);
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