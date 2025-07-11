#ifndef SCREEN_H
#define SCREEN_H

#include "process.h"

void screen_start(const char *name);
void screen_resume(const char *name);
void screen_list(int num_cores, Process **cpu_cores);

#endif
