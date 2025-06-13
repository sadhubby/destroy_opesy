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

typedef struct {
    char name[32];
    int total_prints;
    int finished_print;

    time_t start_time;
    time_t end_time;
    int core_assigned;

    char filename[64];
} Process;

typedef struct {
    Process* process;
} Task;

// Queue
Process* queue[MAX_PROCESSES];
int front = 0, rear = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_is_not_empty = PTHREAD_COND_INITIALIZER;

int stop_scheduler = 0;

void enqueue(Process* p) {
    queue[rear++] = p;
}

Process* dequeue() {
    if (front == rear) return NULL;
    return queue[front++];
}

// Scheduler Thread
void* scheduler_thread(void* arg) {
    Task* task_list = (Task*)arg;
    int task_count = *((int*)&task_list[MAX_PROCESSES]); // count stored at the end

    for (int i = 0; i < task_count; i++) {
        pthread_mutex_lock(&queue_mutex);
        enqueue(task_list[i].process);
        pthread_cond_signal(&queue_is_not_empty);
        pthread_mutex_unlock(&queue_mutex);

        sleep(1);
    }

    pthread_mutex_lock(&queue_mutex);
    stop_scheduler = 1;
    pthread_cond_broadcast(&queue_is_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    return NULL;
}

void log_print(Process* process, int core_id){
    FILE* fp = fopen(process->filename, "a");
    if(!fp) return;

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);

    fprintf(fp, "(%s) Core:%d \"Hello world from %s!\"\n", timestamp, core_id, process->name);
    printf("(%s) Core:%d \"Hello world from %s!\"\n", timestamp, core_id, process->name);
    fclose(fp);

}

int main() {
    int task_count;
    Task task_list[MAX_PROCESSES];
    Process process_list[MAX_PROCESSES];


    strcpy(process_list[0].name, "Process_1");
    strcpy(process_list[0].filename, "process_1.log");
    log_print(&process_list[0], 1);


    pthread_t cores[NUM_CORES];
    int core_ids[NUM_CORES];

    pthread_t scheduler;
    pthread_create(&scheduler, NULL, scheduler_thread, task_list);
    pthread_join(scheduler, NULL);

    for (int i = 0; i < NUM_CORES; i++) {
        pthread_join(cores[i], NULL);

    }

    Process* finished[MAX_PROCESSES];
    for (int i = 0; i < task_count; i++)
        finished[i] = &process_list[i];
    return 0;
}
