#include "process.h"
#include "scheduler.h"
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
    Instruction *inst;

    // if inside a for loop
    if (p->for_depth > 0) {
        ForContext *ctx = &p->for_stack[p->for_depth - 1];
        if (ctx->current_index < ctx->sub_instruction_count) {
            inst = &ctx->sub_instructions[ctx->current_index];
        } else if (ctx->remaining > 0) {
            ctx->remaining--;
            ctx->current_index = 0;
            inst = &ctx->sub_instructions[ctx->current_index];
        } else {
            // when the loop is done, pop from stack
            p->for_depth--;
            p->program_counter++;
            return;
        }
    }
    else
        inst = &p->instructions[p->program_counter];

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
        
        // for
        case FOR: {
            // create a new context
            ForContext *ctx = &p->for_stack[p->for_depth++];
            ctx->repeat_count = inst->repeat_count;
            ctx->remaining = inst->repeat_count - 1;
            ctx->current_index = 0;
            ctx->sub_instructions = inst->sub_instructions;
            ctx->sub_instruction_count = inst->sub_instruction_count;

            inst = &ctx->sub_instructions[ctx->current_index];
            execute_instruction(p);
            break;
        }
        // sleep
        case SLEEP: {
            // sleep for x ticks
            p->state = SLEEPING;
            p->sleep_until_tick = CPU_TICKS + inst->value;
            p->program_counter++;
            return; 
        }

    }

    // if in for loop, move index in for loop
    if (p->for_depth > 0) {
        p->for_stack[p->for_depth - 1].current_index++;
    } else {
        p->program_counter++;
    }

    return;
}

Process **process_table = NULL;
uint32_t num_processes = 0;
uint32_t process_table_size = 0;

void add_process(Process *p) {
    if (num_processes >= process_table_size) {
        process_table_size = process_table_size == 0 ? 8 : process_table_size * 2;
        process_table = realloc(process_table, process_table_size * sizeof(Process *));
    }

    process_table[num_processes++] = p;
}