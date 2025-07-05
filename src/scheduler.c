#include <windows.h>
#include <stdio.h>
#include "scheduler.h"
#include "process.h"

uint64_t CPU_TICKS = 0;
volatile int scheduler_running = 0;
HANDLE scheduler_thread;

DWORD WINAPI scheduler_loop(LPVOID lpParam) {
    while (scheduler_running) {
        CPU_TICKS++;

        // wake up sleeping processes
        for (int i = 0; i < num_processes; i++) {
            Process *p = &(process_table[i]);
            if (p->state == SLEEPING && CPU_TICKS >= p->sleep_until_tick) {
                p->state = READY;
            }
        }

        // to add all d scheduler logic
        // assign ready and run running processes
    }
    return 0;
}

// scheduler thread create
void start_scheduler() {
    scheduler_running = 1;
    scheduler_thread = CreateThread(NULL, 0, scheduler_loop, NULL, 0, NULL);
}

// close scheduler thread
void stop_scheduler() {
    scheduler_running = 0;
    WaitForSingleObject(scheduler_thread, INFINITE);
    CloseHandle(scheduler_thread);
}

void busy_wait_ticks(uint32_t delay_ticks) {
    uint64_t target_tick = CPU_TICKS + delay_ticks;
    while (CPU_TICKS < target_tick) {
        // to add more code here
    }
}