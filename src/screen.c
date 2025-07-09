#include <stdio.h>
#include <string.h>
#include <time.h>
#include "screen.h"

// static Process *process_table[MAX_PROCESSES];
static int process_count = 0;
static int next_pid = 1;

#define yellow "\x1b[33m"
#define green "\x1b[32m"
#define reset "\x1b[0m"

void printColor(const char *color, const char *text);

// print timestamp
void print_timestamp(time_t raw_time) {
    struct tm *timeinfo = localtime(&raw_time);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    printf("%s", buffer);
}

// process-smi
void process_smi(Process *p) {
    printf("\nProcess name: %s\n", p->name);
    printf("ID: %d\n", p->pid);
    printf("Logs:\n");
}

// screen -s
void screen_start(const char *name) {
    // check for count
    if (process_count >= MAX_PROCESSES) {
        printColor(yellow, "Max session limit reached.\n");
        return;
    }

    // check for duplicate
    for (int i = 0; i < process_count; i++) {
        // if (strcmp(process_table[i]->name, name) == 0) {
        //     char buffer[150];
        //     snprintf(buffer, sizeof(buffer), "Screen session '%s' already exists. Use -r to resume.\n", name);
        //     printColor(yellow, buffer);                    
        //     return;
        // }
    }

    // create a new process and add to table
    // Process *p = create_process(name, next_pid++);

    // // print new process
    // printf("Attached to new screen: %s (PID: %d)\n", p->name, p->pid);
    // printf("Type 'exit' to return to main menu.\n");

    // // accept inputs
    // char input[64];
    // while (1) {
    //     printf("[%s] Enter command: ", p->name);
    //     fgets(input, sizeof(input), stdin);
    //     input[strcspn(input, "\n")] = '\0';

    //     if (strcmp(input, "exit") == 0) {
    //         printf("Returning to main menu.\n");
    //         return;
    //     } else if (strcmp(input, "process-smi") == 0) {
    //         process_smi(p);
    //     } else {
    //         printColor(yellow, "Invalid screen command format.\n");
    //     }
    // }
}

void screen_resume(const char *name) {
    
}

void screen_list() {
    
}
