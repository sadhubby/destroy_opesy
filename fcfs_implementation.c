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
} Process;

typedef struct {
    Process* process;
} Task;

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
    Task* task_list = (Task*)arg;
    int task_count = *((int*)&task_list[MAX_PROCESSES]);

    for (int i = 0 ; i < task_count ; i++) {
        pthread_mutex_lock(&queue_mutex);
        enqueue(task_list[i].process) ;
        pthread_cond_signal(&queue_is_not_empty);
        pthread_mutex_unlock(&queue_mutex);

        sleep(1);
    }

    pthread_mutex_lock(&queue_mutex);
    stop_scheduler =1;
    pthread_cond_broadcast(&queue_is_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    return NULL;
}

int main() {
    int task_count ;
    Task task_list[MAX_PROCESSES];
    Process process_list[MAX_PROCESSES];

    pthread_t cores[NUM_CORES];
    int core_ids[NUM_CORES];

    pthread_t scheduler;
    pthread_create(&scheduler,NULL, scheduler_thread, task_list);
    pthread_join(scheduler, NULL);

    for (int i = 0 ; i < NUM_CORES ; i++) {
        pthread_join(cores[i], NULL);
    }

    Process* finished[MAX_PROCESSES];
    for (int i = 0 ; i < task_count ; i++)
        finished[i] =&process_list[i];
    return 0;
}
