#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PROCESS_NAME 50

typedef enum {
    READY,
    RUNNING,
    SLEEPING,
    FINISHED
} ProcessState;

typedef struct {
    char name[MAX_PROCESS_NAME];
    uint16_t value;
} Variable;

typedef enum {
    DECLARE,
    ADD,
    SUBTRACT,
    PRINT,
    SLEEP,
    FOR_START,
    FOR_END
} InstructionType;

typedef struct {
    InstructionType type;
    char arg1[50];
    char arg2[50];
    char arg3[50];
    uint16_t value;
    uint8_t repeat_count; 
} Instruction;

typedef struct {
    char message[50];
    int tick;
} Log;

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

    bool is_in_scren;

} Process;

Process *create_process(const char *name, int pid);

#endif