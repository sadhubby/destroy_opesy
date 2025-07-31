#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include <time.h>

#define MAX_PROCESS_NAME 50
#define MAX_LOOP_DEPTH 3

// track states of the different processes
typedef enum {
    READY,
    RUNNING,
    SLEEPING,
    FINISHED
} ProcessState;

// variables
typedef struct {
    char name[MAX_PROCESS_NAME];
    uint16_t value;
} Variable;

// type of instruction (for is segmented)
typedef enum {
    DECLARE,
    ADD,
    SUBTRACT,
    PRINT,
    SLEEP,
    FOR,
    READ,
    WRITE
} InstructionType;

typedef struct Instruction Instruction;

// defining instructions, args are inside of parentheses, value for constants/sleep, repeat count for for loop
typedef struct Instruction {
    InstructionType type;
    char arg1[50];
    char arg2[50];
    char arg3[50];
    uint16_t value;
    uint8_t repeat_count; 

    struct Instruction *sub_instructions;
    int sub_instruction_count;
} Instruction;

// for logs in screen
typedef struct {
    char message[50];
    int tick;
} Log;

// contents of a for loop
typedef struct {
    int repeat_count;
    int remaining;
    int current_index;
    Instruction *sub_instructions;
    int sub_instruction_count;
} ForContext;

typedef struct{

    char name[MAX_PROCESS_NAME];
    int pid;
    ProcessState state;
    int program_counter;
    uint16_t sleep_until_tick;

    Variable *variables;
    int num_var;
    int variables_capacity;

    Instruction *instructions;
    int num_inst;

    Log *logs;
    int num_logs;

    int start;
    int end;

    bool is_in_screen;
    int in_memory;

    ForContext for_stack[MAX_LOOP_DEPTH];
    int for_depth;

    time_t last_exec_time;
    uint64_t memory_allocation;
    uint64_t mem_base;
    uint64_t mem_limit;

} Process;

Variable *get_variable(Process *p, const char *name);
uint16_t resolve_value(Process *p, const char *arg, uint16_t fallback);
void execute_instruction(Process *p, Config config);
void add_process(Process *p);

void trim(char *str);
Instruction parse_declare(const char *args);
Instruction parse_add_sub(const char *args, int is_add);
Instruction parse_print(const char *args);
Instruction parse_sleep(const char *args);

int parse_instruction_list(const char *instrs, Instruction *out, int max_count);
Instruction parse_for(const char *args);

extern Process **process_table;
extern uint32_t num_processes;
extern uint32_t process_table_size;

Process *generate_dummy_process(Config config);
void print_process_info(Process *p);

#endif
