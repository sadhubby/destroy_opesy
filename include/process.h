#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>

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
    FOR_START,
    FOR_END
} InstructionType;

// defining instructions, args are inside of parentheses, value for constants/sleep, repeat count for for loop
typedef struct {
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

    Instruction *instructions;
    int num_inst;

    Log *logs;
    int num_logs;

    int start;
    int end;

    bool is_in_screen;

    ForContext for_stack[MAX_LOOP_DEPTH];
    int for_depth;

} Process;

Variable *get_variable(Process *p, const char *name);
uint16_t resolve_value(Process *p, const char *arg, uint16_t fallback);
void execute_instruction(Process *p);

#endif