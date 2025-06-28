#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "process.h"

// constructor
Process *create_process(const char *name, int pid) {
    Process *p = malloc(sizeof(Process));
    if (!p) return NULL;
    strncpy(p->name, name, 50);
    p->name[50] = '\0';
    p->pid = pid;
    p->total_prints = 1;
    p->finished_print = 0;
    p->is_finished = 0;
    p->start_time = time(NULL);
    p->core_assigned = -1;

    return p;
}