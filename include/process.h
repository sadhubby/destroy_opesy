#include <time.h>

#ifndef PROCESS_H
#define PROCESS_H

typedef struct{

    char name[32];
    int pid;
    int total_prints;
    int finished_print;

    time_t start_time;
    time_t end_time;
    int core_assigned;

    int burst_time;
    int is_finished;

    char filename[64];

} Process;

Process *create_process(const char *name, int pid);
void destroy_process(Process *p);

#endif