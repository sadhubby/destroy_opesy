#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
// #include <threads.h> check documentations how to put this in cuz not work rn  

// colors use for design
#define yellow "\x1b[33m"
#define green "\x1b[32m"
#define reset "\x1b[0m"

// prototype so as to only see main at top
void header();
void initialize();
void screen();
void scheduler_test();
void scheduler_stop();
void report_util();
void clear();

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
            screen();
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

void screen(){

    printf("Screen command recognized. Doing something\n");

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
