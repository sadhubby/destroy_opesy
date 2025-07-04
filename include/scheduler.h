#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <windows.h>

extern uint64_t CPU_TICKS;

DWORD WINAPI scheduler_loop(LPVOID lpParam);
void start_scheduler();
void stop_scheduler();
void busy_wait_ticks(uint32_t delay_ticks);

#endif