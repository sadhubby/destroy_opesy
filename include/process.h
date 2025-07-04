#include <time.h>
#include "instruction.h"

#ifndef PROCESS_H
#define PROCESS_H

typedef struct{

    char name[50];
    int pid;
    int total_prints;
    int finished_print;
    int is_finished;
    Instruction *instructions;

} Process;

Process *create_process(const char *name, int pid);
void destroy_process(Process *p);

#endif