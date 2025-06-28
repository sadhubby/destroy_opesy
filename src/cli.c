#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "cli.h"
#include "config.h"
#include "screen.h"

// global variables
static bool initialized = false;
static bool running = true;
static Config config;

// colors used for design
#define yellow "\x1b[33m"
#define green "\x1b[32m"
#define reset "\x1b[0m"

// function headers
void printColor(const char *color, const char *text);
void printHeader();
void printDevelopers();
void printCommandList();
void initialize();

// main loop
void runCLI() {
    // print welcome messages
    char command[100];
    printHeader();
    printColor(green, "\nHello! Welcome to PEFE-OS command line!\n\n");
    printDevelopers();
    printf("\n");
    printf("Type 'help' for a list of commands.\n");
    printf("\n");

    while (running) {
        // get command
        printf("[MAIN MENU] Enter command: ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;

        // exit
        if (strcmp(command, "exit") == 0) {
            printf("Exiting...\n");
            running = false;
        }
        // help
        else if (strcmp(command, "help") == 0) {
            printCommandList();
        }
        // initialize
        else if (strcmp(command, "initialize") == 0) {
            initialize();
        }
        // initialize
        else if (strncmp(command, "screen -s ", 10) == 0) {
            screen_start(command + 10);
        }
        // scheduler-start
        else if (strcmp(command, "scheduler-start") == 0) {
            printf("scheduler-start\n");
        }
        // scheduler-stop
        else if (strcmp(command, "scheduler-stop") == 0) {
            printf("scheduler-stop\n");
        }
        // report-util
        else if (strcmp(command, "report-util") == 0) {
            printf("report-util\n");
        }
        // not initialized
        else if (!initialized) {
            printColor(yellow, "You must run 'initialize' first.\n");
        }
        // unknown command
        else {
            printColor(yellow, "Unknown command.\n");
        }
    }
}

// prints with color
void printColor(const char *color, const char *text){
    printf("%s%s%s", color, text, reset);
}

// print ascii header text
void printHeader(){

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

// print list of developer names
void printDevelopers() {
    printColor(yellow, "Developers:\n");
    printf("David, Peter\n");
    printf("De Guzman, Evan\n");
    printf("Soriano, Erizha\n");
    printf("Tano, Fiona\n");
}

// print list of acceptable commands
void printCommandList() {
    printColor(yellow, "List of commands:\n");
    printf("help - prints the list of commands\n");
    printf("initialize - initialize the processor configuration of the application. This must be called before any other command could be recognized, aside from 'exit'\n");
    printf("exit - terminates the console\n");
    printf("screen -s <process name> - create a new process\n");
    printf("screen -ls - list all running processes\n");
    printf("scheduler-start - continuously generates a batch of processes for the CPU scheduler. Each process is accessible via the 'screen' command.\n");
    printf("scheduler-stop - stops generating processes\n");
    printf("report-util - for generating CPU utilization report\n");
}

// initialize
void initialize(){
    load_config(&config);
    printColor(yellow, "Configuration loaded successfully.\n");
    
    printf("  num-cpu: %d\n", config.num_cpu);
    printf("  scheduler: %s\n", config.scheduler);
    printf("  quantum-cycles: %d\n", config.quantum_cycles);
    printf("  batch-process-freq: %d\n", config.batch_process_freq);
    printf("  min-ins: %d\n", config.min_ins);
    printf("  max-ins: %d\n", config.max_ins);
    printf("  delays-per-exec: %d\n", config.delay_per_exec);
    initialized = true;
}