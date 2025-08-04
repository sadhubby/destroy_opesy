#ifndef SCREEN_H
#define SCREEN_H

#include <stdbool.h>
#include "process.h"


typedef struct{
    char name[50];
    int current_line;
    int total_lines;
    char timestamp[30];
    bool active;
} ScreenSession;


void screen_start(const char *name, int memory_size);
void screen_resume(const char *name);
void screen_list(int num_cores, Process **cpu_cores, int finished_count, Process **finished_processes);
void screen_create_with_code(const char *command_args);
bool is_valid_memory_size(int memory_size);
void report_utilization(int num_cores, Process **cpu_cores, int finished_count, Process **finished_processes);
#endif
