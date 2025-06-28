#include <time.h>

#ifndef PROCESS_H
#define PROCESS_H

typedef struct{

    char name[50];
    int pid;
    int total_prints;
    int finished_print;
    int is_finished;

    struct {
        time_t timestamp;
        int core_id;
        char message[128];
    } logs[100];

    int log_count;

} Process;

Process *create_process(const char *name, int pid);
void destroy_process(Process *p);

#endif