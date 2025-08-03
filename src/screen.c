#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "screen.h"
#include "scheduler.h"

static int process_count = 0;

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
void screen_process_smi(Process *p) {
    printf("\nProcess name: %s\n", p->name);
    printf("ID: %d\n", p->pid);
    printf("Logs:\n");
}

// screen -s
void screen_start(const char *name) {

    // check for duplicate
    for (int i = 0; i < process_count; i++) {
         if (strcmp(process_table[i]->name, name) == 0) {
             char buffer[150];
             snprintf(buffer, sizeof(buffer), "Screen session '%s' already exists. Use -r to resume.\n", name);
             printColor(yellow, buffer);                    
             return;
         }
    }

    // create a new process and add to table
    Process *p = malloc(sizeof(Process));
    if (!p) {
        printColor(yellow, "Failed to allocate memory for new process.\n");
        return;
    }
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    p->pid = process_count + 1;
    p->program_counter = 0;
    p->num_inst = rand() % 1000 + 200; // Randomized instruction length, or from config
    p->last_exec_time = time(NULL);
    // Initialize logs, etc.

    add_process(p);

    // // print new process
    printf("Attached to new screen: %s (PID: %d)\n", p->name, p->pid);
    printf("Type 'exit' to return to main menu.\n");

    // // accept inputs
    char input[64];
    while (1) {
        printf("[%s] Enter command: ", p->name);
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
        printf("Returning to main menu.\n");
            return;
        } else if (strcmp(input, "process-smi") == 0) {
            screen_process_smi(p);
        } else {
            printColor(yellow, "Invalid screen command format.\n");
        }
    }
}

void screen_resume(const char *name) {
    //in case tapos na yung process
    int finished_count = get_finished_count();
    Process **finished_processes = get_finished_processes();
    for (int i = 0; i < finished_count; i++) {
        if (finished_processes[i] && strcmp(finished_processes[i]->name, name) == 0) {
            printf("Process %s not found.\n", name);
            return;
        }
    }

    
    for (uint32_t i = 0; i < num_processes; i++) {
        if (process_table[i] && strcmp(process_table[i]->name, name) == 0) {
            Process *p = process_table[i];

            printf("Resumed screen: %s (PID: %d)\n", p->name, p->pid);
            printf("Type 'exit' to return to main menu.\n");

            char input[64];
            while (1) {
                printf("[%s] Enter command: ", p->name);
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';

                if (strcmp(input, "exit") == 0) {
                    printf("Returning to main menu.\n");
                    return;
                } else if (strcmp(input, "process-smi") == 0) {
                    screen_process_smi(p);
                } else {
                    printColor(yellow, "Invalid screen command format.\n");
                }
            }
        }
    }

    
    printf("Process %s not found.\n", name);
}

void screen_create_with_code(const char *command_args) {
    char process_name[50];
    int memory_size;
    char instructions[1024];

    // instruction format-  <process_name> <memory_size> "<instructions>"
    if (sscanf(command_args, "%s %d \"%[^\"]\"", process_name, &memory_size, instructions) != 3) {
        printColor(yellow, "invalid command format\n");
        return;
    }

    // count the number of instructions
    int count = 1;
    for (int i = 0; instructions[i]; i++) {
        if (instructions[i] == ';') count++;
    }

    if (count < 1 || count > 50) {
        printColor(yellow, "invalid command\n");
        return;
    }

    // check for duplicate pname
    for (int i = 0; i < process_count; i++) {
        if (strcmp(process_table[i]->name, process_name) == 0) {
            char buffer[150];
            snprintf(buffer, sizeof(buffer), "Screen session '%s' already exists. Use -r to resume.\n", process_name);
            printColor(yellow, buffer);
            return;
        }
    }

    // allocate and create the process
    Process *p = malloc(sizeof(Process));
    if (!p) {
        printColor(yellow, "Failed to allocate memory for new process.\n");
        return;
    }

    strncpy(p->name, process_name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    p->pid = process_count + 1;
    p->program_counter = 0;
    p->last_exec_time = time(NULL);
    p->code = strdup(instructions); // assumes `char *code;` is in Process struct
    p->memory_size = memory_size;
    p->num_inst = count;

    add_process(p);

    printf("Created process '%s' with code:\n%s\n", process_name, instructions);
}


void screen_list(int num_cores, Process **cpu_cores, int finished_count, Process **finished_processes) {
    int used = 0;

    // count used
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] != NULL) {
            used++;
        }
    }

    // calculate
    int available = num_cores - used;
    double utilization = (num_cores > 0) ? (100.0 * used / num_cores) : 0.0;

    // print generic report
    
    printf("CPU Utilization: %.2f%%\n", utilization);
    printf("Cores used: %d\n", used);
    printf("Cores available: %d\n", available);

    // print running processes
    printf("\nRunning Processes\n");
    printf("%-16s %-24s %-12s %-10s\n", "Name", "Last Exec Time", "Core", "PC/Total");
    for (int i = 0; i < num_cores; i++) {
        if (cpu_cores[i] != NULL) {
            Process *p = cpu_cores[i];
            char timebuf[32];
            struct tm *tm_info = localtime(&p->last_exec_time);
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
            printf("P%-16s %-24s %-10d %d/%d\n", p->name, timebuf, i, p->program_counter + 1, p->num_inst);
        }
    }

    // print finished processes
    printf("\nFinished Processes\n");
    printf("%-16s %-24s %-12s %-10s\n", "Name", "Last Exec Time", "Core", "PC/Total");
    for (int i = 0; i < finished_count; i++) {
        if (finished_processes[i] != NULL) {
            // printf("[DEBUG] Finished process: %s (PID: %d)\n", finished_processes[i]->name, finished_processes[i]->pid);

            Process *p = finished_processes[i];
            char timebuf[32];
            struct tm *tm_info = localtime(&p->last_exec_time);
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
            printf("P%-16s %-24s %-10s     %d/%d\n", p->name, timebuf, "Finished", p->program_counter, p->num_inst);
        }
    }
}
