#ifndef SCREEN_H
#define SCREEN_H

#include "process.h"

typedef struct{
    char name[50];
    int current_line;
    int total_lines;
    char timestamp[30];
    bool active;
} ScreenSession;


void screen_start(const char *name);
void screen_resume(const char *name);
void screen_list(int num_cores, Process **cpu_cores, int finished_count, Process **finished_processes);

#endif
