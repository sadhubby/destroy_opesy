#ifndef INSTRUCTION_H
#define INSTRUCTION_H

typedef enum {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR
} InstructionType;

typedef struct Instruction Instruction;

struct Instruction {
    InstructionType type;
    char args[3][32];
    int repeat_count;
    int sub_count;
    struct Instruction *sub_instructions;
};

Instruction generate_random_instruction(void);

#endif