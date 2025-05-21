#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
// #include <windows.h>
// #include <threads.h> check documentations how to put this in cuz not work rn  

// colors use for design
#define yellow "\x1b[33m"
#define green "\x1b[32m"
#define reset "\x1b[0m"

// constants
#define MAX_SESSIONS 10

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
void scheduler_stop();
void report_util();
void clear();
void print_color(const char *color, const char *text);
void get_current_timestamp(char *buffer, size_t size);
void draw_session(ScreenSession *session);

// design decision
void print_color(const char *color, const char *text);

int main(){

    char command [100];
    header();
    printf("");
    print_color(green, "Hello! Welcome to PEFE-OS command line!\n");
    print_color(yellow, "Type 'exit' to quit, 'clear' to clear the screen\n");
    
    while(1){
    
    printf("Enter command: \n");
    fgets(command, sizeof(command), stdin);
    command[strcspn(command, "\n")] = '\0'; 
    
        if (strcmp(command, "exit") == 0) {
            printf("Exiting...");
            exit(0);
        } else if (strcmp(command, "clear") == 0) {
            clear();
        } else if (strcmp(command, "initialize") == 0) {
            initialize();
        } else if (strcmp(command, "screen") == 0) {
            print_color(yellow, "Invalid screen command. Usage:\n");
            print_color(yellow, "  screen -s <name>    (create new screen session)\n");
            print_color(yellow, "  screen -r <name>    (resume existing screen session)\n");
        } else if (strncmp(command, "screen -s", 9) == 0 || strncmp(command, "screen -r", 9) == 0) {
            screen(command);
        } else if (strcmp(command, "scheduler-test") == 0) {
            scheduler_test();
        } else if (strcmp(command, "scheduler-stop") == 0) {
            scheduler_stop();
        } else if (strcmp(command, "report-util") == 0) {
            report_util();
        } else {
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

    printf("Initialize command recognized. Doing something\n");
    
}

void get_current_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%m/%d/%Y, %I:%M:%S %p", t);
}

void draw_session(ScreenSession *session) {
    printf("\nScreen Session Info\n");
    printf("Process Name: %s\n", session->name);
    printf("Instruction Line: %d / %d\n", session->current_line, session->total_lines);
    printf("Timestamp: %s\n", session->timestamp);

    char cmd[100];
    while (1) {
        printf("[%s] Enter command: ", session->name);
        fgets(cmd, sizeof(cmd), stdin);
        cmd[strcspn(cmd, "\n")] = '\0';

        if (strcmp(cmd, "exit") == 0) {
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

            ScreenSession *new_session = &sessions[session_count++];
            strcpy(new_session->name, name);
            new_session->current_line = rand() % 100 + 1;
            new_session->total_lines = 100;
            get_current_timestamp(new_session->timestamp, sizeof(new_session->timestamp));
            new_session->active = true;

            draw_session(new_session);
        } else if (strcmp(dash, "-r") == 0) {
            for (int i = 0; i < session_count; i++) {
                if (strcmp(sessions[i].name, name) == 0 && sessions[i].active) {
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

    printf("Scheduler test command recognized. Doing something\n");

}

void scheduler_stop(){

    printf("Scheduler stop command recognized. Doing something\n");

}

void report_util(){

    printf("Report util command recognized. Doing something\n");  

}

void clear(){

    printf("\x1b[2J\x1b[H");
    header();
    printf("");
    print_color(green, "Hello! Welcome to PEFE-OS command line!\n");
    print_color(yellow, "Type 'exit' to quite, 'clear' to clear the screen\n");
}
