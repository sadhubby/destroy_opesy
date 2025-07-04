#include <windows.h>
#include <stdio.h>
#include "scheduler.h"

uint64_t CPU_TICKS = 0;
volatile int scheduler_running = 0;
HANDLE scheduler_thread;

DWORD WINAPI scheduler_loop(LPVOID lpParam) {
    while (scheduler_running) {
        CPU_TICKS++;
        // to add all d scheduler logic
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