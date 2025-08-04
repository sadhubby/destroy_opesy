#ifndef BACKING_STORE_H
#define BACKING_STORE_H

#include "process.h"
#include <windows.h>

extern CRITICAL_SECTION backing_store_cs;

void write_process_to_backing_store(Process *p);
Process* read_first_process_from_backing_store();
void remove_first_process_from_backing_store();
void print_backing_store_contents();

#endif