#ifndef SCREEN_H
#define SCREEN_H

#include "process.h"

#define MAX_PROCESSES 128

void screen_start(const char *name);
void screen_resume(const char *name);
void screen_list();

#endif
