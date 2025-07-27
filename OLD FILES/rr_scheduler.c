#include "scheduler.h"
#include "scheduler_utils.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

pthread_mutex_t rr_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rr_cond = PTHREAD_COND_INITIALIZER;

int rr_front = 0, rr_rear = 0;
Process* rr_queue[MAX_PROCESSES];
int stop_rr_scheduler = 0;

void rr_enqueue(Process* p) {
    rr_queue[rr_rear++ % MAX_PROCESSES] = p;
}

Process* rr_dequeue() {
    if (rr_front == rr_rear) return NULL;
    return rr_queue[rr_front++ % MAX_PROCESSES];
}

void create_batch_processes(int batch_size) {
    for (int i = 0; i < batch_size; i++) {
        int idx = find_free_process_slot();
        if (idx < 0) break;
        int burst = get_random_burst(system_config.min_ins, system_config.max_ins);
        char pname[16];
        sprintf(pname, "p%0d", ++global_process_counter); 
        create_process(&process_list[idx], idx, burst);
        strcpy(process_list[idx].name, pname); 
        task_list[idx].process = &process_list[idx];
        rr_enqueue(&process_list[idx]);
    }
}

void* rr_core_worker(void* arg) {
    int core_id = *((int*)arg);

    while (1) {
        pthread_mutex_lock(&rr_mutex);
        while (rr_front == rr_rear && !stop_rr_scheduler) {
            pthread_cond_wait(&rr_cond, &rr_mutex);
        }

        if (rr_front == rr_rear && stop_rr_scheduler) {
            pthread_mutex_unlock(&rr_mutex);
            break;
        }

        Process* p = rr_dequeue();
        pthread_mutex_unlock(&rr_mutex);

        if (p && !p->is_finished) {
            p->core_assigned = core_id;
            if (p->start_time == 0)
                p->start_time = time(NULL);

            int quantum = system_config.quantum_cycles;
            int time_slice = (p->burst_time - p->finished_print >= quantum)
                            ? quantum
                            : p->burst_time - p->finished_print;

            for (int i = 0; i < time_slice; i++) {
                p->finished_print++;
                usleep(system_config.delay_per_exec * 1000); // delay in microseconds
            }

            if (p->finished_print >= p->burst_time) {
                p->is_finished = 1;
                p->end_time = time(NULL);
                if (finished_count < MAX_FINISHED_PROCESSES) {
                finished_list[finished_count++] = *p;
                }
                create_batch_processes(system_config.batch_process_freq);
                pthread_cond_broadcast(&rr_cond);
            } else {
                pthread_mutex_lock(&rr_mutex);
                rr_enqueue(p);
                pthread_cond_signal(&rr_cond);
                pthread_mutex_unlock(&rr_mutex);
            }
        }
    }

    return NULL;
}

void start_rr_scheduler() {
    srand(time(NULL));
    rr_front = rr_rear = 0;
    stop_rr_scheduler = 0;

    create_batch_processes(system_config.batch_process_freq);

    pthread_t cores[128];
    int core_ids[128];

    int num_cores = system_config.num_cpu;
    for (int i = 0; i < num_cores; i++) {
        core_ids[i] = i + 1;
        pthread_create(&cores[i], NULL, rr_core_worker, &core_ids[i]);
    }

    for (int i = 0; i < num_cores; i++) {
        pthread_detach(cores[i]);
    }

    printf("Round Robin Scheduler started.\n");
}

void stop_rr_scheduler_now() {
    pthread_mutex_lock(&rr_mutex);
    stop_rr_scheduler = 1;
    rr_front = rr_rear = 0;
    pthread_cond_broadcast(&rr_cond);
    pthread_mutex_unlock(&rr_mutex);

    printf("Round Robin Scheduler stopped.\n");
}

void rr_screen_ls() {
    printf("------------------------------------------------------------\n");
    printf("\nRunning processes:\n");
    // printf("+----------------+------------------------+---------+------------+\n");
    // printf("|  Process Name  |   Start Timestamp      | Core    | Progress   |\n");
    // printf("+----------------+------------------------+---------+------------+\n");

    // Show only running processes assigned to a core and not finished
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].burst_time == 0) continue;
        if (process_list[i].core_assigned == -1 || process_list[i].is_finished) continue;

        char timestamp[64] = "-";
        if (process_list[i].start_time != 0) {
            struct tm* t = localtime(&process_list[i].start_time);
            strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);
        }
        printf("%-10s ( %-22s )  Core %-2d  %4d / %-6d \n",
               process_list[i].name,
               timestamp,
               process_list[i].core_assigned,
               process_list[i].finished_print,
               process_list[i].burst_time);
    }
    // printf("+----------------+------------------------+---------+------------+\n\n");

    printf("\nFinished processes:\n");
    // printf("+----------------+------------------------+----------+------------+\n");
    // printf("|  Process Name  |   End Timestamp        | Status   | Progress   |\n");
    // printf("+----------------+------------------------+----------+------------+\n");

    for (int i = 0; i < finished_count; i++) {
    Process *proc = &finished_list[i];
    char timestamp[64] = "-";
    if (proc->end_time != 0) {
        struct tm* t = localtime(&proc->end_time);
        strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);
    }
    printf("%-10s ( %-22s )  Finished   %4d / %-6d \n",
           proc->name,
           timestamp,
           proc->finished_print,
           proc->burst_time);
}
     printf("------------------------------------------------------------\n\n");

}