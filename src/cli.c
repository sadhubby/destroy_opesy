#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "cli.h"
#include "config.h"
#include "screen.h"
#include "scheduler.h"
#include "process.h"

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
void test_instructions();

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
        // not initialized
        else if (!initialized) {
            printColor(yellow, "You must run 'initialize' first.\n");
        }
        // screen -s
        else if (strncmp(command, "screen -s ", 10) == 0) {
            screen_start(command + 10);
        }
        // screen -r
        else if (strncmp(command, "screen -r ", 10) == 0) {
            screen_resume(command + 10);
        }
        // screen -ls
        else if (strcmp(command, "screen -ls") == 0) {
            screen_list();
        }
        // scheduler-start
        else if (strcmp(command, "scheduler-start") == 0) {
            start_scheduler();
        }
        // scheduler-stop
        else if (strcmp(command, "scheduler-stop") == 0) {
            stop_scheduler();
        }
        // report-util
        else if (strcmp(command, "report-util") == 0) {
            printf("report-util\n");
        }
        // test-instructions
        else if (strcmp(command, "test-instructions") == 0) {
            test_instructions();
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

// just to test when instructions work
void test_instructions() {
    printf("This is a DEBUG command!\n");

    // make new process p
    Process p;
    memset(&p, 0, sizeof(Process));
    strcpy(p.name, "test_proc");

    // set instructions and variables
    Instruction instructions[10];
    Variable variables[10];
    p.instructions = instructions;
    p.variables = variables;
    p.num_inst = 5;
    p.num_var = 0;

    // DECLARE x = 10
    p.instructions[0].type = DECLARE;
    strcpy(p.instructions[0].arg1, "x");
    p.instructions[0].value = 10;

    // DECLARE y = 5
    p.instructions[1].type = DECLARE;
    strcpy(p.instructions[1].arg1, "y");
    p.instructions[1].value = 5;

    // ADD z = x + y
    p.instructions[2].type = ADD;
    strcpy(p.instructions[2].arg1, "z");
    strcpy(p.instructions[2].arg2, "x");
    strcpy(p.instructions[2].arg3, "y");

    // SUBTRACT w = z - y
    p.instructions[3].type = SUBTRACT;
    strcpy(p.instructions[3].arg1, "w");
    strcpy(p.instructions[3].arg2, "z");
    strcpy(p.instructions[3].arg3, "y");

    // PRINT w
    p.instructions[4].type = PRINT;
    strcpy(p.instructions[4].arg1, "w");

    // SLEEP for 2 ticks
    p.instructions[5].type = SLEEP;
    p.instructions[5].value = 2;

    // FOR loop: repeat 3 times { ADD x = x + x; PRINT x; }
    Instruction for_sub[2];
    for_sub[0].type = ADD;
    strcpy(for_sub[0].arg1, "x");
    strcpy(for_sub[0].arg2, "x");
    strcpy(for_sub[0].arg3, "x");
    for_sub[1].type = PRINT;
    strcpy(for_sub[1].arg1, "x");

    p.instructions[6].type = FOR;
    p.instructions[6].repeat_count = 3;
    p.instructions[6].sub_instructions = for_sub;
    p.instructions[6].sub_instruction_count = 2;

    // PRINT x after the loop
    p.instructions[7].type = PRINT;
    strcpy(p.instructions[7].arg1, "x");

    p.num_inst = 8;
    p.program_counter = 0;

    // Execute all instructions with debug output
    while (p.program_counter < p.num_inst) {
        printf("Executing instruction %d: type=%d\n", p.program_counter, p.instructions[p.program_counter].type);
        execute_instruction(&p);
        printf("Variables after instruction %d:\n", p.program_counter - 1);
        for (int i = 0; i < p.num_var; i++) {
            printf("  %s = %u\n", p.variables[i].name, p.variables[i].value);
        }
    }
}