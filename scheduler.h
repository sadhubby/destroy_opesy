#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <time.h>

#define MAX_PROCESSES 10
#define NUM_CORES 4
#define BURST_TIME 100
#define TIME_QUANTUM 10
#define MAX_FINISHED_PROCESSES (MAX_PROCESSES * 10)

typedef struct{

    char name[32];
    int total_prints;
    int finished_print;

    time_t start_time;
    time_t end_time;
    int core_assigned;

    int burst_time;
    int is_finished;

    char filename[64];

} Process;

typedef struct{
    Process* process;
} Task;

extern Process process_list [MAX_PROCESSES];
extern Task task_list[MAX_PROCESSES];
extern Process finished_list[MAX_FINISHED_PROCESSES];
extern int finished_count;
void start_scheduler(void);
void stop_scheduler_now(void);
void start_rr_scheduler(void);
void stop_rr_scheduler_now(void);
void rr_screen_ls(void);

#endif

