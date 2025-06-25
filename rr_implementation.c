#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

void log_print(Process* p, int core_id);


pthread_mutex_t rr_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rr_cond = PTHREAD_COND_INITIALIZER;

int rr_front = 0, rr_rear = 0;
Process* rr_queue[MAX_PROCESSES];
int stop_rr_scheduler = 0;

void rr_enqueue(Process* p) {
    rr_queue[rr_rear++] = p;
}

Process* rr_dequeue() {
    if (rr_front == rr_rear) return NULL;
    return rr_queue[rr_front++];
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

            int time_slice = (p->burst_time - p->finished_print >= TIME_QUANTUM)
                            ? TIME_QUANTUM
                            : p->burst_time - p->finished_print;

            for (int i = 0; i < time_slice; i++) {
                log_print(p, core_id);
                p->finished_print++;
                usleep(50000); 
            }

            if (p->finished_print >= p->burst_time) {
                p->is_finished = 1;
                p->end_time = time(NULL);
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
    for (int i = 0; i < MAX_PROCESSES; i++) {
        sprintf(process_list[i].name, "process_%d", i + 1);
        sprintf(process_list[i].filename, "process_%d.txt", i + 1);
        process_list[i].burst_time = BURST_TIME;
        process_list[i].total_prints = BURST_TIME;
        process_list[i].finished_print = 0;
        process_list[i].is_finished = 0;
        process_list[i].core_assigned = -1;
        process_list[i].start_time = 0;
        process_list[i].end_time = 0;

        task_list[i].process = &process_list[i];
    }

    for (int i = 0; i < MAX_PROCESSES; i++) {
        rr_enqueue(task_list[i].process);
    }

    pthread_t cores[NUM_CORES];
    int core_ids[NUM_CORES];

    for (int i = 0; i < NUM_CORES; i++) {
        core_ids[i] = i + 1;
        pthread_create(&cores[i], NULL, rr_core_worker, &core_ids[i]);
    }

    for (int i = 0; i < NUM_CORES; i++) {
        pthread_detach(cores[i]);
    }

    printf("Round Robin Scheduler started.\n");
}

void stop_rr_scheduler_now() {
    pthread_mutex_lock(&rr_mutex);
    stop_rr_scheduler = 1;
    pthread_cond_broadcast(&rr_cond);
    pthread_mutex_unlock(&rr_mutex);

    printf("Round Robin Scheduler stopped.\n");
}

void rr_screen_ls() {
    printf("+----------------+------------------------+---------------+------------+\n");
    printf("|  Process Name  |   Last Timestamp       | Assigned Core | Progress   |\n");
    printf("+----------------+------------------------+---------------+------------+\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        char timestamp[64] = "-";
        if (process_list[i].end_time != 0) {
            struct tm* t = localtime(&process_list[i].end_time);
            strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);
        }

        printf("| %-14s | %-22s | Core %-7d | %3d/%-6d |\n",
               process_list[i].name,
               timestamp,
               process_list[i].core_assigned,
               process_list[i].finished_print,
               process_list[i].total_prints);
    }

    printf("+----------------+------------------------+---------------+------------+\n");
}