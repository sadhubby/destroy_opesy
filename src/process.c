#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "process.h"

Process *create_process(const char *name, int pid, int total_prints, time_t start_time) {
    Process *p = malloc(sizeof(Process));
    if (!p) return NULL;
    strncpy(p->name, name, 50);
    p->name[50] = '\0';
    p->pid = pid;
    p->total_prints = total_prints;
    p->finished_print = 0;
    p->is_finished = 0;
    p->start_time = start_time;
    p->core_assigned = -1;

    return p;
}

void destroy_process(Process *p) {
    free(p);
}