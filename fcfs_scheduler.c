#include "scheduler.h"
#include "scheduler_utils.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

Process* queue[MAX_PROCESSES];
int front = 0, rear = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_is_not_empty = PTHREAD_COND_INITIALIZER;

int stop_scheduler = 0;

void enqueue(Process* p) {
    if ((rear + 1) % MAX_PROCESSES == front) {
        printf("FCFS queue overflow! Cannot enqueue more processes.\n");
        return;
    }
    queue[rear] = p;
    rear = (rear + 1) % MAX_PROCESSES;
}

Process* dequeue() {
    if (front == rear) return NULL; // Queue is empty
    Process* p = queue[front];
    front = (front + 1) % MAX_PROCESSES;
    return p;
}

extern int global_process_counter;

void* scheduler_thread(void* arg) {
    for (int i = 0; i < system_config.batch_process_freq; i++) {
        int idx = find_free_process_slot();
        if (idx < 0) break;
        int burst = get_random_burst(system_config.min_ins, system_config.max_ins);
        char pname[16];
        sprintf(pname, "p%0d", ++global_process_counter); // p0001, p0002, ...
        create_process(&process_list[idx], idx, burst);
        strcpy(process_list[idx].name, pname);
        task_list[idx].process = &process_list[idx];

        pthread_mutex_lock(&queue_mutex);
        enqueue(task_list[idx].process);
        pthread_cond_signal(&queue_is_not_empty);
        pthread_mutex_unlock(&queue_mutex);
    }

    pthread_mutex_lock(&queue_mutex);
    pthread_cond_broadcast(&queue_is_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    return NULL;
}
void* core_worker(void* arg) {   int core_id = *((int*)arg);

    while (1) {
        pthread_mutex_lock(&queue_mutex);
        while (front == rear && !stop_scheduler) {
            pthread_cond_wait(&queue_is_not_empty, &queue_mutex);
        }

        if (front == rear && stop_scheduler) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        Process* p = dequeue();
        if (p == NULL) {
            pthread_mutex_unlock(&queue_mutex);
            continue;
        }
        pthread_mutex_unlock(&queue_mutex);

        if (p && !p->is_finished) {
            p->core_assigned = core_id;
            p->start_time = time(NULL);
            for (int i = 0; i < p->burst_time; i++) {
                p->finished_print++;
                usleep(system_config.delay_per_exec * 1000);
            }
            p->end_time = time(NULL);
            p->is_finished = 1;
            if (finished_count < MAX_FINISHED_PROCESSES) {
            finished_list[finished_count++] = *p;
            }
            // Create a new batch after finishing
            if (!stop_scheduler) {
                for (int b = 0; b < system_config.batch_process_freq; b++) {
                    int idx = find_free_process_slot();
                    if (idx < 0) break;
                    int burst = get_random_burst(system_config.min_ins, system_config.max_ins);
                    char pname[16];
                    sprintf(pname, "p%0d", ++global_process_counter);
                    create_process(&process_list[idx], idx, burst);
                    strcpy(process_list[idx].name, pname);
                    task_list[idx].process = &process_list[idx];

                    pthread_mutex_lock(&queue_mutex);
                    enqueue(task_list[idx].process);
                    pthread_cond_signal(&queue_is_not_empty);
                    pthread_mutex_unlock(&queue_mutex);
                }
            }
        }
    }

    return NULL;
}

void start_scheduler() {
    srand(time(NULL));
    front = rear = 0;
    stop_scheduler = 0;

    pthread_t scheduler;
    pthread_create(&scheduler, NULL, scheduler_thread, NULL);

    pthread_t cores[128];
    int core_ids[128];

    int num_cores = system_config.num_cpu;
    for (int i = 0; i < num_cores; i++) {
        core_ids[i] = i + 1;
        pthread_create(&cores[i], NULL, core_worker, &core_ids[i]);
    }

    pthread_detach(scheduler);
    for (int i = 0; i < num_cores; i++) {
        pthread_detach(cores[i]);
    }

    printf("FCFS Scheduler started.\n");
}

void stop_scheduler_now() {
    pthread_mutex_lock(&queue_mutex);
    stop_scheduler = 1;
    front = rear = 0;
    pthread_cond_broadcast(&queue_is_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    printf("FCFS Scheduler stopped.\n");
}

void screen_ls() {
    printf("------------------------------------------------------------\n");
    printf("\nRunning processes:\n");
    // printf("+----------------+------------------------+---------+------------+\n");
    // printf("|  Process Name  |   Start Timestamp      | Core    | Progress   |\n");
    // printf("+----------------+------------------------+---------+------------+\n");

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
    printf("-------------------------------------------------------------------\n");
}