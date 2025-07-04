#include "scheduler.h"

uint64_t CPU_TICKS = 0;

void tick() {
    CPU_TICKS++;
}