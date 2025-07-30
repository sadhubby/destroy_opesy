#ifndef BACKING_STORE_H
#define BACKING_STORE_H

#include "process.h"

void write_process_to_backing_store(Process *p);
int read_all_processes_from_backing_store(Process ***out_array);
void remove_process_from_backing_store(int pid);
void print_backing_store_contents();

#endif