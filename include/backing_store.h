#ifndef BACKING_STORE_H
#define BACKING_STORE_H

#include <stdio.h>
#include "process.h"
#include <windows.h>

extern CRITICAL_SECTION backing_store_cs;

void init_backing_store();
static FILE* ensure_backing_store(const char* mode);
void write_process_to_backing_store(Process *p);
Process* read_first_process_from_backing_store();
void remove_first_process_from_backing_store();
void print_backing_store_contents();

#endif