// #include "scheduler.h"
// #include "scheduler_utils.h"
// #include "config.h"
// #include "instruction.h"
// #include "barebones.h"
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <pthread.h>
// #include <unistd.h>
// #include <time.h>

// Process* queue[MAX_PROCESSES];
// int front = 0, rear = 0;

// pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_cond_t queue_is_not_empty = PTHREAD_COND_INITIALIZER;

// int stop_scheduler = 0;

// void enqueue(Process* p) {
//     if ((rear + 1) % MAX_PROCESSES == front) {
//         // Queue overflow, should not happen if logic is correct
//         return;
//     }
//     queue[rear] = p;
//     rear = (rear + 1) % MAX_PROCESSES;
// }

// Process* dequeue() {
//     if (front == rear) return NULL; // Queue is empty
//     Process* p = queue[front];
//     front = (front + 1) % MAX_PROCESSES;
//     return p;
// }

// extern int global_process_counter;

// void* scheduler_thread(void* arg) {
//     while (!stop_scheduler) {
//         int free_slots = 0;
//         for (int i = 0; i < MAX_PROCESSES; i++) {
//             if (process_list[i].burst_time == 0)
//                 free_slots++;
//         }
//         int to_create = (system_config.batch_process_freq < free_slots) ? system_config.batch_process_freq : free_slots;

//         for (int i = 0; i < to_create; i++) {
//             int idx = find_free_process_slot();
//             if (idx < 0) break;
//             create_process(&process_list[idx], idx, 0);
//             char pname[16];
//             sprintf(pname, "p%0d", ++global_process_counter);
//             strcpy(process_list[idx].name, pname);

//             generate_process_instructions(process_list[idx].instructions, system_config.min_ins, system_config.max_ins, process_list[idx].name);

//             process_list[idx].burst_time = 0;
//             for (char *p = process_list[idx].instructions; *p; p++) {
//                 if (*p == '\n') process_list[idx].burst_time++;
//             }

//             task_list[idx].process = &process_list[idx];

//             pthread_mutex_lock(&queue_mutex);
//             enqueue(task_list[idx].process);
//             pthread_cond_signal(&queue_is_not_empty);
//             pthread_mutex_unlock(&queue_mutex);
//         }
//         sleep(1); // Wait before checking again
//     }
//     return NULL;
// }

// void execute_process_instructions(Process *proc) {
//     char instr_copy[256000];
//     strcpy(instr_copy, proc->instructions);
//     char *instr = strtok(instr_copy, "\n");
//     while (instr && proc->finished_print < proc->burst_time) {
//         barebones(instr);
//         proc->finished_print++;
//         usleep(system_config.delay_per_exec * 1000);
//         instr = strtok(NULL, "\n");
//     }
// }

// void* core_worker(void* arg) { 
//     int core_id = *((int*)arg);

//     while (1) {
//         pthread_mutex_lock(&queue_mutex);
//         while (front == rear && !stop_scheduler) {
//             pthread_cond_wait(&queue_is_not_empty, &queue_mutex);
//         }

//         if (front == rear && stop_scheduler) {
//             pthread_mutex_unlock(&queue_mutex);
//             break;
//         }

//         Process* p = dequeue();
//         pthread_mutex_unlock(&queue_mutex);

//         if (p && !p->is_finished) {
//             p->core_assigned = core_id;
//             p->start_time = time(NULL);

//             // Execute the instructions!
//             execute_process_instructions(p);

//             // Only mark as finished and reset if truly done!
//             if (p->finished_print >= p->burst_time) {
//                 p->end_time = time(NULL);
//                 p->is_finished = 1;
//                 if (finished_count < MAX_FINISHED_PROCESSES) {
//                     finished_list[finished_count++] = *p;
//                 }
//                 // Reset the slot for reuse
//                 p->burst_time = 0;
//                 p->core_assigned = -1;
//                 p->start_time = 0;
//                 p->end_time = 0;
//                 p->finished_print = 0;
//                 p->is_finished = 0;
//                 p->name[0] = '\0';
//                 p->instructions[0] = '\0';
//             }
//         }
//     }

//     return NULL;
// }

// void start_scheduler() {
//     srand(time(NULL));
//     front = rear = 0;
//     stop_scheduler = 0;

//     pthread_t scheduler;
//     pthread_create(&scheduler, NULL, scheduler_thread, NULL);

//     pthread_t cores[128];
//     int core_ids[128];

//     int num_cores = system_config.num_cpu;
//     for (int i = 0; i < num_cores; i++) {
//         core_ids[i] = i + 1;
//         pthread_create(&cores[i], NULL, core_worker, &core_ids[i]);
//     }

//     pthread_detach(scheduler);
//     for (int i = 0; i < num_cores; i++) {
//         pthread_detach(cores[i]);
//     }

//     printf("FCFS Scheduler started.\n");
// }

// void stop_scheduler_now() {
//     pthread_mutex_lock(&queue_mutex);
//     stop_scheduler = 1;
//     front = rear = 0;
//     pthread_cond_broadcast(&queue_is_not_empty);
//     pthread_mutex_unlock(&queue_mutex);

//     printf("FCFS Scheduler stopped.\n");
// }

// void screen_ls() {
//     printf("------------------------------------------------------------\n");
//     printf("\nRunning processes:\n");

//     int core_seen[128] = {0}; // Support up to 128 cores

//     for (int i = 0; i < MAX_PROCESSES; i++) {
//         if (process_list[i].burst_time == 0) continue;
//         if (process_list[i].core_assigned == -1 || process_list[i].is_finished) continue;
//         int cid = process_list[i].core_assigned;
//         if (cid > 0 && cid < 128 && core_seen[cid]) continue; // already showed a process for this core
//         core_seen[cid] = 1;

//         char timestamp[64] = "-";
//         if (process_list[i].start_time != 0) {
//             struct tm* t = localtime(&process_list[i].start_time);
//             strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);
//         }
//         printf("%-10s ( %-22s )  Core %-2d  %4d / %-6d \n",
//                process_list[i].name,
//                timestamp,
//                process_list[i].core_assigned,
//                process_list[i].finished_print,
//                process_list[i].burst_time);
//     }

//     printf("\nFinished processes:\n");
//     for (int i = 0; i < finished_count; i++) {
//         Process *proc = &finished_list[i];
//         char timestamp[64] = "-";
//         if (proc->end_time != 0) {
//             struct tm* t = localtime(&proc->end_time);
//             strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);
//         }
//         printf("%-10s ( %-22s )  Finished   %4d / %-6d \n",
//                proc->name,
//                timestamp,
//                proc->finished_print,
//                proc->burst_time);
//     }
//     printf("------------------------------------------------------------\n\n");
// }
#include "scheduler.h"
#include "scheduler_utils.h"
#include "config.h"
#include "instruction.h"
#include "barebones.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// --- Unlimited Ready Queue (Linked List) ---
typedef struct ReadyNode {
    Process* proc;
    struct ReadyNode* next;
} ReadyNode;

ReadyNode* ready_front = NULL;
ReadyNode* ready_rear = NULL;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_is_not_empty = PTHREAD_COND_INITIALIZER;

int stop_scheduler = 0;

// --- Ready Queue Functions ---
void enqueue(Process* p) {
    ReadyNode* node = malloc(sizeof(ReadyNode));
    node->proc = p;
    node->next = NULL;
    if (ready_rear) {
        ready_rear->next = node;
        ready_rear = node;
    } else {
        ready_front = ready_rear = node;
    }
}

Process* dequeue() {
    if (!ready_front) return NULL;
    ReadyNode* node = ready_front;
    Process* p = node->proc;
    ready_front = node->next;
    if (!ready_front) ready_rear = NULL;
    free(node);
    return p;
}

extern int global_process_counter;

// --- Scheduler Thread: Create new processes as slots free up ---
void* scheduler_thread(void* arg) {
    while (!stop_scheduler) {
        int free_slots = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_list[i].burst_time == 0)
                free_slots++;
        }
        int to_create = (system_config.batch_process_freq < free_slots) ? system_config.batch_process_freq : free_slots;

        for (int i = 0; i < to_create; i++) {
            int idx = find_free_process_slot();
            if (idx < 0) break;
            create_process(&process_list[idx], idx, 0);
            char pname[16];
            sprintf(pname, "p%0d", ++global_process_counter);
            strcpy(process_list[idx].name, pname);

            generate_process_instructions(process_list[idx].instructions, system_config.min_ins, system_config.max_ins, process_list[idx].name);

            process_list[idx].burst_time = 0;
            for (char *p = process_list[idx].instructions; *p; p++) {
                if (*p == '\n') process_list[idx].burst_time++;
            }

            task_list[idx].process = &process_list[idx];

            pthread_mutex_lock(&queue_mutex);
            enqueue(task_list[idx].process);
            pthread_cond_signal(&queue_is_not_empty);
            pthread_mutex_unlock(&queue_mutex);
        }
        sleep(1); // Wait before checking again
    }
    return NULL;
}

void execute_process_instructions(Process *proc) {
    char instr_copy[256000];
    strcpy(instr_copy, proc->instructions);
    char *instr = strtok(instr_copy, "\n");
    while (instr && proc->finished_print < proc->burst_time) {
        barebones(instr);
        proc->finished_print++;
        usleep(system_config.delay_per_exec * 1000);
        instr = strtok(NULL, "\n");
    }
}

// --- Each core runs one process to completion before taking another ---
void* core_worker(void* arg) { 
    int core_id = *((int*)arg);

    while (1) {
        pthread_mutex_lock(&queue_mutex);
        while (!ready_front && !stop_scheduler) {
            pthread_cond_wait(&queue_is_not_empty, &queue_mutex);
        }

        if (!ready_front && stop_scheduler) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        Process* p = dequeue();
        pthread_mutex_unlock(&queue_mutex);

        if (p && !p->is_finished) {
            p->core_assigned = core_id;
            p->start_time = time(NULL);

            execute_process_instructions(p);

            if (p->finished_print >= p->burst_time) {
                p->end_time = time(NULL);
                p->is_finished = 1;
                if (finished_count < MAX_FINISHED_PROCESSES) {
                    finished_list[finished_count++] = *p;
                }
                // Reset slot for reuse
                p->burst_time = 0;
                p->core_assigned = -1;
                p->start_time = 0;
                p->end_time = 0;
                p->finished_print = 0;
                p->is_finished = 0;
                p->name[0] = '\0';
                p->instructions[0] = '\0';
            }
        }
    }

    return NULL;
}

void start_scheduler() {
    srand(time(NULL));
    stop_scheduler = 0;

    pthread_t scheduler;
    pthread_create(&scheduler, NULL, scheduler_thread, NULL);

    pthread_t cores[NUM_CORES];
    int core_ids[NUM_CORES];

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
    // Clean up ready queue
    while (ready_front) {
        ReadyNode* node = ready_front;
        ready_front = node->next;
        free(node);
    }
    ready_rear = NULL;
    pthread_cond_broadcast(&queue_is_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    printf("FCFS Scheduler stopped.\n");
}

void screen_ls() {
    printf("------------------------------------------------------------\n");
    printf("\nRunning processes:\n");

    int core_seen[NUM_CORES+1] = {0}; // core id starts from 1

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].burst_time == 0) continue;
        if (process_list[i].core_assigned == -1 || process_list[i].is_finished) continue;
        int cid = process_list[i].core_assigned;
        if (cid > 0 && cid <= NUM_CORES && core_seen[cid]) continue; // already showed a process for this core
        core_seen[cid] = 1;

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