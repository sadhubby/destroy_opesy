#include <stdlib.h>
#include <string.h>
#include "instruction.h"

// variables
const char *vars[] = {"x", "y", "z", "a", "b", "c"};

// generate random instructions
Instruction generate_random_instruction(void) {
    // randomize
    Instruction inst;
    inst.type = rand() % 6;
    inst.repeat_count = 0;
    inst.sub_count = 0;
    inst.sub_instructions = NULL;

    switch (inst.type) {
        // print
        case PRINT:
            snprintf(inst.args[0], sizeof(inst.args[0]), "Hello world");
            break;
        // declare
        case DECLARE:
            snprintf(inst.args[0], sizeof(inst.args[0]), "%s", vars[rand() % 6]);
            snprintf(inst.args[1], sizeof(inst.args[1]), "%d", rand() % 100);
            break;
        // add
        case ADD:
        // subtract
        case SUBTRACT:
            snprintf(inst.args[0], sizeof(inst.args[0]), "%s", vars[rand() % 6]);
            snprintf(inst.args[1], sizeof(inst.args[1]), "%s", vars[rand() % 6]);
            snprintf(inst.args[2], sizeof(inst.args[2]), "%d", rand() % 100); 
            break;
        // sleep
        case SLEEP:
            snprintf(inst.args[0], sizeof(inst.args[0]), "%d", 1 + rand() % 5);
            break;
        // for
        case FOR: {
            inst.repeat_count = 1 + rand() % 3;
            inst.sub_count = 1 + rand() % 3;
            inst.sub_instructions = malloc(sizeof(Instruction) * inst.sub_count);
            for (int i = 0; i < inst.sub_count; i++) {
                inst.sub_instructions[i] = generate_random_instruction();
            }
            break;
        }
    }

    return inst;
}
