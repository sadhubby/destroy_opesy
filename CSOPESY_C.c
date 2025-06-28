#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "scheduler.h"
#include "config.h"
#include "report_util.c"
#include "barebones.c"


// #include <windows.h>
// #include <threads.h> check documentations how to put this in cuz not work rn

// colors use for design
#define yellow "\x1b[33m"
#define green "\x1b[32m"
#define reset "\x1b[0m"

// constants
#define MAX_SESSIONS 10

Process process_list[MAX_PROCESSES];
Task task_list[MAX_PROCESSES];

Process finished_list[MAX_FINISHED_PROCESSES];
int finished_count = 0;
// session structure
typedef struct {
    char name[50];
    int current_line;
    int total_lines;
    char timestamp[30];
    bool active;
} ScreenSession;

ScreenSession sessions[MAX_SESSIONS];
int session_count = 0;

// prototype so as to only see main at top
void header();
void initialize();
void screen(const char *input);
void scheduler_test();
void rr_scheduler_test();
void rr_scheduler_stop();
void scheduler_stop();
void report_util();
void clear();
void print_color(const char *color, const char *text);
void get_current_timestamp(char *buffer, size_t size);
void draw_session(ScreenSession *session);
void screen_ls();

// design decision
void print_color(const char *color, const char *text);

int main(){

    char command [100];
    bool is_initialized = false;

    printf("\x1b[2J\x1b[H");
    header();
    //printf("");
    print_color(green, "\nHello! Welcome to PEFE-OS command line!\n");
    print_color(yellow, "Type 'exit' to quit, 'clear' to clear the screen\n");
    
    while(1){
    
    printf("[MAIN MENU] Enter command: ");
    fgets(command, sizeof(command), stdin);
    command[strcspn(command, "\n")] = '\0'; 
    
        if (strcmp(command, "exit") == 0) {
            printf("Exiting...");
            exit(0);
        } 
        else if (strcmp(command, "clear") == 0) {
            clear();
        } 
        else if (strcmp(command, "initialize") == 0) {
            initialize();
            is_initialized = true;
        } 
        else if (!is_initialized) {
            print_color(yellow, "You must run 'initialize' first.\n");
        } 
        else if (strcmp(command, "screen") == 0) {
            print_color(yellow, "Invalid screen command. Usage:\n");
            print_color(yellow, "  screen -s <name>    (create new screen session)\n");
            print_color(yellow, "  screen -r <name>    (resume existing screen session)\n");
        } 
        else if (strncmp(command, "screen -s", 9) == 0 || strncmp(command, "screen -r", 9) == 0) {
            screen(command);
        } 
        else if (strcmp(command, "scheduler -start") == 0) {
             // code for scheduler -start with different scheduling algos
            if(strcmp(system_config.scheduler, "fcfs") == 0) {
                scheduler_test();
            } else if(strcmp(system_config.scheduler, "rr") == 0) {
                rr_scheduler_test();
            } else {
                print_color(yellow, "No screen sessions found.\n");
            }
        } 
        else if (strcmp(command, "scheduler -stop") == 0) {


            if(strcmp(system_config.scheduler, "fcfs") == 0) {
                scheduler_stop();
            } else if(strcmp(system_config.scheduler, "rr") == 0) {
                rr_scheduler_stop();
            } else {
                print_color(yellow, "No screen sessions found.\n");
            }
        } 
        else if (strcmp(command, "report-util") == 0) {
            report_util();
        } 
        else if(strcmp(command, "screen -ls")==0){
            // code for screen_ls with different scheduling algos

            if(strcmp(system_config.scheduler, "fcfs") == 0) {
                screen_ls();
            } else if(strcmp(system_config.scheduler, "rr") == 0) {
                rr_screen_ls();
            } else {
                print_color(yellow, "No screen sessions found.\n");
            }
        }
        else {
            print_color(yellow, "Unknown command.\n");
        }
        
    }
    
    return 0;
}

void header(){

    FILE *file = fopen("ascii.txt", "r");
    if(file == NULL){
        perror("Error opening file");
    }

    char line[256];
    while(fgets(line, sizeof(line), file)){
        printf("%s", line);
    }
    fclose(file);

}

void print_color(const char *color, const char *text){
    printf("%s%s%s", color, text, reset);
}

void initialize(){
    load_config();
    print_color(yellow, "Configuration loaded successfully.\n");
    
    printf("  num-cpu: %d\n", system_config.num_cpu);
    printf("  scheduler: %s\n", system_config.scheduler);
    printf("  quantum-cycles: %d\n", system_config.quantum_cycles);
    printf("  batch-process-freq: %d\n", system_config.batch_process_freq);
    printf("  min-ins: %d\n", system_config.min_ins);
    printf("  max-ins: %d\n", system_config.max_ins);
    printf("  delays-per-exec: %d\n", system_config.delay_per_exec);
}

void get_current_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%m/%d/%Y, %I:%M:%S %p", t);
}

void draw_session(ScreenSession *session) {
    printf("\x1b[2J\x1b[H");

    Process *proc = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (strcmp(process_list[i].name, session->name) == 0) {
            proc = &process_list[i];
            break;
        }
    }

    if (!proc) {
        print_color(yellow, "Process not found.\n");
        return;
    }

    char cmd[100];
    while (1) {
        if (proc->is_finished) {
            session->active = false;
            printf("\nProcess %s has finished.\n", session->name);
            return;
        }

        printf("[%s] Enter command: ", session->name);
        fgets(cmd, sizeof(cmd), stdin);
        cmd[strcspn(cmd, "\n")] = '\0';

        if (strcmp(cmd, "process-smi") == 0) {
            printf("\nProcess name: %s\n", session->name);
            printf("ID: %d\n", (int)(proc - process_list) + 1);
            printf("Logs:\n");

            for (int i = 0; i < 2; i++) {
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char timestamp[32];
                strftime(timestamp, sizeof(timestamp), "(%m/%d/%Y %I:%M:%S%p)", t);
                printf("%s Core:%d \"Hello world from %s!\"\n", timestamp,
                    proc->core_assigned == -1 ? 0 : proc->core_assigned,
                    session->name);
            }

            session->current_line += rand() % 25 + 1;
            if (session->current_line > session->total_lines)
                session->current_line = session->total_lines;

            printf("\nCurrent instruction line: %d\n", session->current_line);
            printf("Lines of code: %d\n", session->total_lines);

            if (proc->is_finished || proc->finished_print >= proc->burst_time) {
                session->active = false;
                proc->is_finished = 1;
                printf("\nFinished!\n");
                return;
            }

        } else if (strcmp(cmd, "exit") == 0) {
            printf("\n");
            return;
        } else {
            char buffer[150];
            snprintf(buffer, sizeof(buffer), "Command '%s' not recognized inside screen.\n", cmd);
            print_color(yellow, buffer);
        }
    }
}



void screen(const char *input) {
    char dash[3], name[50];

        if (sscanf(input, "screen %2s %49s", dash, name) == 2) {
            if (strcmp(dash, "-s") == 0) {
            // check for duplicate
            for (int i = 0; i < session_count; i++) {
                if (strcmp(sessions[i].name, name) == 0 && sessions[i].active) {
                    char buffer[150];
                    snprintf(buffer, sizeof(buffer), "Screen session '%s' already exists. Use -r to resume.\n", name);
                    print_color(yellow, buffer);
                    return;
                }
            }

            if (session_count >= MAX_SESSIONS) {
                print_color(yellow, "Max session limit reached.\n");
                return;
            }

            ScreenSession *new_session = &sessions[session_count];
            strcpy(new_session->name, name);
            new_session->current_line = 0;
            new_session->total_lines = 100;
            get_current_timestamp(new_session->timestamp, sizeof(new_session->timestamp));
            new_session->active = true;

            if (session_count < MAX_PROCESSES) {
                Process *proc = &process_list[session_count];
                sprintf(proc->name, "%s", name);
                sprintf(proc->filename, "%s.log", name);
                proc->burst_time = BURST_TIME;
                proc->total_prints = proc->burst_time;
                proc->finished_print = 0;
                proc->is_finished = 0;
                proc->core_assigned = -1;
                proc->start_time = 0;
                proc->end_time = 0;

                task_list[session_count].process = proc;
                session_count++;
            } else {
                print_color(yellow, "Maximum number of processes reached.\n");
                return;
            }


            draw_session(new_session);
        } else if (strcmp(dash, "-r") == 0) {
            for (int i = 0; i < session_count; i++) {
                if (strcmp(sessions[i].name, name) == 0) {
                    if (!sessions[i].active || process_list[i].is_finished) {
                        char msg[100];
                        snprintf(msg, sizeof(msg), "Process %s not found.\n", name);
                        print_color(yellow, msg);
                        return;
                    }
                    draw_session(&sessions[i]);
                    return;
                }
            }
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "Session '%s' not found.\n", name);
            print_color(yellow, buffer);
        } else {
            print_color(yellow, "Invalid option. Use -s or -r.\n");
        }
    } else {
        print_color(yellow, "Invalid screen command format.\n");
    }
}

void scheduler_test(){

    print_color(green, "Starting scheduler...");
    start_scheduler();

}

void rr_scheduler_test(){

    print_color(green, "Starting scheduler...");
    start_rr_scheduler();

}

void rr_scheduler_stop(){

    print_color(yellow, "Stopping scheduler...");
    stop_rr_scheduler_now();

}

void scheduler_stop(){

    print_color(yellow, "Stopping scheduler...");
    stop_scheduler_now();

}

void report_util() {
    // Terminal UI header
    printf("------------------------------------------------------------\n");
    printf("\nCPU Utilization Report\n");
    printf("Cores Used: %d / %d\n", system_config.num_cpu, system_config.num_cpu);
    printf("\nRunning processes:\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process *p = &process_list[i];
        if (p->burst_time == 0) continue;
        if (p->core_assigned == -1 || p->is_finished) continue;

        char timestamp[64] = "-";
        if (p->start_time != 0) {
            struct tm *t = localtime(&p->start_time);
            strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);
        }

        printf("%-10s ( %-22s )  Core %-2d  %4d / %-6d \n",
               p->name, timestamp, p->core_assigned,
               p->finished_print, p->burst_time);
    }

    printf("\nFinished processes:\n");

    for (int i = 0; i < finished_count; i++) {
        Process *p = &finished_list[i];
        char timestamp[64] = "-";
        if (p->end_time != 0) {
            struct tm *t = localtime(&p->end_time);
            strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", t);
        }

        printf("%-10s ( %-22s )  Finished   %4d / %-6d \n",
               p->name, timestamp, p->finished_print, p->burst_time);
    }

    printf("------------------------------------------------------------\n");

    // File write
    write_report_to_file("csopesy-log.txt");
    printf(">> Report generated at csopesy-log.txt!\n");
}


void clear(){

    printf("\x1b[2J\x1b[H");
    header();
    //printf("");
    print_color(green, "\nHello! Welcome to PEFE-OS command line!\n");
    print_color(yellow, "Type 'exit' to quit, 'clear' to clear the screen\n");
}
