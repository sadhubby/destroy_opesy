// gcc fcfs.c -o fcfs -lpthread
// ./fcfs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_PROCESSES 10
#define NUM_CORES 4
#define BURST_TIME 100

int log_to_file = 1;

typedef struct {
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

typedef struct {
    Process* process;
} Task;

typedef struct {
    Task task_list[MAX_PROCESSES];
    int task_count;
} TaskBundle;


Process process_list[MAX_PROCESSES]; 
Task task_list[MAX_PROCESSES]; 

Process* queue [MAX_PROCESSES];
int front = 0,rear = 0;

pthread_mutex_t queue_mutex =PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_is_not_empty =PTHREAD_COND_INITIALIZER;

int stop_scheduler = 0;

void enqueue(Process* p){
    queue[rear++] = p;
}

Process* dequeue(){
    if (front ==rear ) return NULL;
    return queue[front++];
}


void* scheduler_thread(void* arg) {
    TaskBundle* bundle = (TaskBundle*)arg;
    Task* task_list = bundle->task_list;
    int task_count = bundle->task_count;

    for (int i = 0 ; i < task_count ; i++) {
        pthread_mutex_lock(&queue_mutex);
        enqueue(task_list[i].process) ;
        pthread_cond_signal(&queue_is_not_empty);
        pthread_mutex_unlock(&queue_mutex);

       // sleep(1);
    }

    pthread_mutex_lock(&queue_mutex);
    stop_scheduler =1;
    pthread_cond_broadcast(&queue_is_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    return NULL;
}

void log_print(Process* process, int core_id){

    if(!log_to_file) return;

    FILE* fp = fopen(process->filename, "a");
    if(!fp) return;

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);

    fprintf(fp, "(%s) Core:%d \"Hello world from %s!\"\n", timestamp, core_id, process->name);
    // printf("(%s) Core:%d \"Hello world from %s!\"\n", timestamp, core_id, process->name);
    fclose(fp);

}

void* core_worker(void* arg) {
    int core_id = *((int*)arg);

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
        pthread_mutex_unlock(&queue_mutex);

        if (p) {
            p->core_assigned = core_id;
            p->start_time = time(NULL);
            for(int i = 0; i < p->burst_time; i++){
                log_print(p, core_id);
                p->finished_print++;
                usleep(50000); 
            }
            p->end_time = time(NULL);
            p->is_finished = 1;
        
        }
    }

    return NULL;
}


void start_scheduler(){
       for (int i = 0; i < MAX_PROCESSES; i++) {
        sprintf(process_list[i].name, "process_%d", i + 1);
        sprintf(process_list[i].filename, "process_%d.log", i + 1);
        process_list[i].burst_time = BURST_TIME;
        process_list[i].total_prints = process_list[i].burst_time;
        process_list[i].finished_print = 0;
        process_list[i].is_finished = 0;
        task_list[i].process = &process_list[i];
    }

    TaskBundle bundle;
    bundle.task_count = MAX_PROCESSES;
    memcpy(bundle.task_list, task_list, sizeof(task_list));

    pthread_t scheduler;
    pthread_create(&scheduler, NULL, scheduler_thread, &bundle);

    pthread_t cores[NUM_CORES];
    int core_ids[NUM_CORES];

    for (int i = 0; i < NUM_CORES; i++) {
        core_ids[i] = i + 1;
        pthread_create(&cores[i], NULL, core_worker, &core_ids[i]);
    }

    pthread_join(scheduler, NULL);
    for (int i = 0; i < NUM_CORES; i++) {
        pthread_join(cores[i], NULL);
    }

    printf("\nScheduler run completed.\n");

}

void stop_scheduler_now(){
    pthread_mutex_lock(&queue_mutex);
    stop_scheduler = 1;
    pthread_cond_broadcast(&queue_is_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    printf("Scheduler stopped. \n");
}


void screen_ls() {

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
