#include "process.h"
#include <stdio.h>
#include <string.h>

// retain in uint16 bounds
#define CLAMP_UINT16(x) ((x) > 65535 ? 65535 : (x))

// get variable given a process and a variable name
Variable *get_variable(Process *p, const char *name) {
    // iterate through each variable
    for (int i = 0; i < p->num_var; i++) {
        if (strcmp(p->variables[i].name, name) == 0) {
            return &p->variables[i];
        }
    }

    // otherwise assign new variable with a value of 0
    strcpy(p->variables[p->num_var].name, name);
    p->variables[p->num_var].value = 0;
    return &p->variables[p->num_var++];
}

//  for null values
uint16_t resolve_value(Process *p, const char *arg, uint16_t fallback) {
    if (arg[0] == '\0') return fallback;
    Variable *v = get_variable(p, arg);
    return v ? v->value : fallback;
}

void execute_instruction(Process *p) {
    if (p->program_counter >= p->num_inst) return;

    Instruction *inst = &p->instructions[p->program_counter];

    switch (inst->type) {
        // declare
        case DECLARE: {
            Variable *v = get_variable(p, inst->arg1);
            v->value = CLAMP_UINT16(inst->value);
            break;
        }
        // add
        case ADD: {
            Variable *dest = get_variable(p, inst->arg1);
            uint16_t v2 = resolve_value(p, inst->arg2, inst->value);
            uint16_t v3 = resolve_value(p, inst->arg3, inst->value);
            dest->value = CLAMP_UINT16(v2 + v3);
            break;
        }
        // subtract
        case SUBTRACT: {
            Variable *dest = get_variable(p, inst->arg1);
            uint16_t v2 = resolve_value(p, inst->arg2, inst->value);
            uint16_t v3 = resolve_value(p, inst->arg3, inst->value);
            int32_t result = (int32_t)v2 - (int32_t)v3;
            dest->value = result < 0 ? 0 : (uint16_t)result;
            break;
        }
        // print
        case PRINT: {
            Variable *v = get_variable(p, inst->arg1);
            if (v) {
                printf("Hello world from %s! Value of %s = %u\n", p->name, v->name, v->value);
            } else {
                printf("Hello world from %s!\n", p->name);
            }
            break;
        }
    }

    p->program_counter++;
}