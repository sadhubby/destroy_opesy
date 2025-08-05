#include "process.h"
#include "scheduler.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Thread-safe PID counter
volatile long next_pid = 1;

// retain in uint16 bounds
#define CLAMP_UINT16(x) ((x) > 65535 ? 65535 : (x))

// get variable given a process and a variable name
Variable *get_variable(Process *p, const char *name) {
    if (!p || !name || !p->variables) {
        printf("[ERROR] Invalid arguments to get_variable: p=%p, name=%p, variables=%p\n",
               (void*)p, (void*)name, p ? (void*)p->variables : NULL);
        return NULL;
    }

    char trimmed_name[MAX_PROCESS_NAME];
    strncpy(trimmed_name, name, MAX_PROCESS_NAME - 1);
    trimmed_name[MAX_PROCESS_NAME - 1] = '\0';
    // Remove ALL whitespace (not just leading/trailing)
    trim(trimmed_name);
    // Remove any leading/trailing whitespace again (defensive)
    int i = 0, j = 0;
    while (trimmed_name[i]) {
        if (!isspace((unsigned char)trimmed_name[i])) {
            trimmed_name[j++] = trimmed_name[i];
        }
        i++;
    }
    trimmed_name[j] = '\0';

    // iterate through each variable
    for (int i = 0; i < p->num_var; i++) {
        if (strcmp(p->variables[i].name, trimmed_name) == 0) {
            return &p->variables[i];
        }
    }

    // Grow variable array if needed, but with safety checks
    if (p->num_var >= p->variables_capacity) {
        if (p->variables_capacity >= 1000000) {
            printf("[ERROR] Variable capacity too large for process %d\n", p->pid);
            return NULL;
        }
        int new_cap = p->variables_capacity * 2;
        Variable *new_vars = malloc(new_cap * sizeof(Variable));  // Use malloc instead of realloc
        if (!new_vars) {
            printf("[ERROR] Failed to allocate new variables array for process %d!\n", p->pid);
            return NULL;
        }
        // Copy old data and free old array
        memcpy(new_vars, p->variables, p->variables_capacity * sizeof(Variable));
        memset(new_vars + p->variables_capacity, 0, (new_cap - p->variables_capacity) * sizeof(Variable));
        free(p->variables);
        p->variables = new_vars;
        p->variables_capacity = new_cap;
        // Debug message removed for cleaner output
    }
    strcpy(p->variables[p->num_var].name, trimmed_name);
    p->variables[p->num_var].value = 0;
    return &p->variables[p->num_var++];
}

//  for null values
uint16_t resolve_value(Process *p, const char *arg, uint16_t fallback) {
    if (arg[0] == '\0') return fallback;
    Variable *v = get_variable(p, arg);
    return v ? v->value : fallback;
}

void execute_instruction(Process *p, Config config) {
    // Add validation checks
    if (!p) {
        printf("[ERROR] Null process pointer in execute_instruction\n");
        return;
    }

    // Validate process structure
    if (p->program_counter < 0 || !p->instructions || !p->variables) {
        printf("[ERROR] Invalid process state: PC=%d, instructions=%p, variables=%p\n",
               p->program_counter, (void*)p->instructions, (void*)p->variables);
        return;
    }

    //guard for out of bounds with better logging
    if (p->program_counter >= p->num_inst) {
        printf("[DEBUG] Process %s reached end: PC=%d, num_inst=%d\n",
               p->name, p->program_counter, p->num_inst);
        return;
    }

    Instruction *inst = &p->instructions[p->program_counter];
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

    switch (inst->type) {
        // declare
       case DECLARE: {
            Variable *v = get_variable(p, inst->arg1);
            if (v) v->value = CLAMP_UINT16(inst->value);
            break;
        }
        // add
        case ADD: {
            // FIX: Resolve values FIRST to avoid realloc invalidating the dest pointer.
            uint16_t v2 = resolve_value(p, inst->arg2, 0);
            uint16_t v3 = resolve_value(p, inst->arg3, 0);
            Variable *dest = get_variable(p, inst->arg1); // Get dest pointer LAST.
            if (dest) {
                dest->value = CLAMP_UINT16(v2 + v3);
            }
            break;
        }
        // subtract
        case SUBTRACT: {
            // FIX: Resolve values FIRST to avoid realloc invalidating the dest pointer.
            uint16_t v2 = resolve_value(p, inst->arg2, 0);
            uint16_t v3 = resolve_value(p, inst->arg3, 0);
            Variable *dest = get_variable(p, inst->arg1); // Get dest pointer LAST.
            if (dest) {
                dest->value = CLAMP_UINT16(v2 - v3);
            }
            break;
        }
        // print
        case PRINT: {
            // Variable *v = get_variable(p, inst->arg1);
            // if (v) {
            //     printf("Hello world from %s! Value of %s = %u\n", p->name, v->name, v->value);
            // } else {
            //     printf("Hello world from %s!\n", p->name);
            // }
            // break;
        }
        
        // for
        case FOR: {
            if (p->for_depth >= MAX_LOOP_DEPTH) {
                printf("[ERROR] Maximum loop depth exceeded in process %d\n", p->pid);
                p->program_counter++;
                break;
            }
            ForContext *ctx = &p->for_stack[p->for_depth++];
            ctx->repeat_count = inst->repeat_count;
            ctx->remaining = inst->repeat_count - 1;
            ctx->current_index = 0;
            ctx->sub_instructions = inst->sub_instructions;
            ctx->sub_instruction_count = inst->sub_instruction_count;
            
            // Execute first instruction of the loop immediately
            if (ctx->sub_instruction_count > 0) {
                inst = &ctx->sub_instructions[ctx->current_index];
                execute_instruction(p, config);
            } else {
                // Empty loop body, just increment program counter
                p->for_depth--;
                p->program_counter++;
            }
            break;
        }
        // sleep
        case SLEEP: {
            // sleep for x ticks
            p->state = SLEEPING;
            p->sleep_until_tick = CPU_TICKS + inst->value;
            break;
        }

        // read from memory
        case READ: {
            Variable *dest = get_variable(p, inst->arg1);
            if (dest) {
                // Get the memory address from arg2 (could be variable or literal)
                uint16_t addr = resolve_value(p, inst->arg2, 0);
                // Read value from memory at addr (assuming 0 if not initialized)
                dest->value = read_from_memory(p, addr);
            }
            break;
        }

        // write to memory
        case WRITE: {
            // Get the memory address from arg1 (could be variable or literal)
            uint16_t addr = resolve_value(p, inst->arg1, 0);
            // Write value directly to memory at addr
            write_to_memory(p, addr, inst->value);
            break;
        }
        // read
        // case READ: {
        //     uint32_t address = 0;
        //     sscanf(inst->arg2, "%x", &address);
        //     int success = 0;
        //     uint16_t value = memory_read(address, p, &success);
        //     if (success) {
        //         Variable *v = get_variable(p, inst->arg1);
        //         if (v) v->value = value;
        //     }
        //     break;
        // }
        // // writee
        // case WRITE: {
        //     uint32_t address = 0;
        //     sscanf(inst->arg1, "%x", &address);
        //     uint16_t value = CLAMP_UINT16(inst->value);
        //     memory_write(address, value, p);
        //     break;
        // }

    }

    // Record the time of this instruction execution
    if (p->state != SLEEPING)
        p->last_exec_time = time(NULL);

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

// trim whitepsace
void trim(char *str) {
    char *end;

    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0)
        return;

    end = str + strlen(str) - 1;

    while (end > str && isspace((unsigned char)*end))
        end--;
        
    *(end+1) = 0;
}

// DECLARE(var, value)
Instruction parse_declare(const char *args) {
    Instruction inst = {0};
    inst.type = DECLARE;
    char var[50];
    int value;
    sscanf(args, "%49[^,],%d", var, &value);
    trim(var);
    strcpy(inst.arg1, var);
    inst.value = value;
    return inst;
}

// ADD(var1, var2/value, var3/value)
Instruction parse_add_sub(const char *args, int is_add) {
    Instruction inst = {0};
    inst.type = is_add ? ADD : SUBTRACT;
    char var1[50], var2[50], var3[50];
    int v2, v3;
    sscanf(args, "%49[^,],%49[^,],%49[^,]", var1, var2, var3);
    trim(var1); trim(var2); trim(var3);
    strcpy(inst.arg1, var1);

    // make sure var3 isn't empty
    if (var3[0] == '\0') {
        strcpy(var3, "0");
    }

    // Parse as raw ints first then variables
    if (sscanf(var2, "%d", &v2) == 1) {
        inst.arg2[0] = '\0';
        inst.value = v2;
    } else {
        strcpy(inst.arg2, var2);
    }
    
    if (sscanf(var3, "%d", &v3) == 1) {
        inst.arg3[0] = '\0';
        inst.value = v3;
    } else {
        strcpy(inst.arg3, var3);
    }
    return inst;
}

// PRINT("msg" + var)
Instruction parse_print(const char *args) {
    Instruction inst = {0};
    inst.type = PRINT;

    // Extract the variable
    const char *plus = strchr(args, '+');
    if (plus) {
        strcpy(inst.arg1, plus + 1);
        trim(inst.arg1);
    } else {
        inst.arg1[0] = '\0';
    }
    return inst;
}

// SLEEP(X)
Instruction parse_sleep(const char *args) {
    Instruction inst = {0};
    inst.type = SLEEP;
    int ticks = 0;
    sscanf(args, "%d", &ticks);
    inst.value = ticks;
    return inst;
}

int parse_instruction_list(const char *instrs, Instruction *out, int max_count);

// FOR([instructions], repeats)
Instruction parse_for(const char *args) {
    Instruction inst = {0};
    inst.type = FOR;

    // Find brackets
    const char *lbracket = strchr(args, '[');
    const char *rbracket = strchr(args, ']');

    // check for missing or malformed brackets
    if (!lbracket || !rbracket || rbracket <= lbracket) {
        return inst;
    }

    // Copy instruction list substring safely
    size_t instr_len = rbracket - lbracket - 1;
    char instr_list[256];
    if (instr_len >= sizeof(instr_list)) instr_len = sizeof(instr_list) - 1;
    strncpy(instr_list, lbracket + 1, instr_len);
    instr_list[instr_len] = '\0';

    // Parse repeat count
    int repeats = 1;
    const char *repeat_ptr = rbracket + 1;
    while (*repeat_ptr && (isspace((unsigned char)*repeat_ptr) || *repeat_ptr == ',')) repeat_ptr++;
    if (sscanf(repeat_ptr, "%d", &repeats) == 1) {
        inst.repeat_count = repeats;
    } else {
        inst.repeat_count = 1;
    }
    int count = 0;

    // Count how many instructions there are
    char instr_list_copy[256];
    strncpy(instr_list_copy, instr_list, sizeof(instr_list_copy));
    instr_list_copy[sizeof(instr_list_copy)-1] = '\0';
    char *token = strtok(instr_list_copy, ";");

    while (token) {
        count++;
        token = strtok(NULL, ";");
    }

    if (count == 0)
        return inst;

    Instruction *sub_instructions = malloc(sizeof(Instruction) * count);
    int actual_count = parse_instruction_list(instr_list, sub_instructions, count);
    inst.sub_instructions = sub_instructions;
    inst.sub_instruction_count = actual_count;

    return inst;
}
// read
Instruction parse_read(const char *args) {
    Instruction inst = {0};
    inst.type = READ;
    char var[50], addr[50];
    sscanf(args, "%49[^,],%49s", var, addr);
    trim(var); trim(addr);
    strcpy(inst.arg1, var);
    strcpy(inst.arg2, addr);
    return inst;
}
// write
Instruction parse_write(const char *args) {
    Instruction inst = {0};
    inst.type = WRITE;
    char addr[50];
    int value;
    sscanf(args, "%49[^,],%d", addr, &value);
    trim(addr);
    strcpy(inst.arg1, addr);
    inst.value = value;
    return inst;
}

// parse list of instructions
int parse_instruction_list(const char *instrs, Instruction *out, int max_count) {
    char buf[256];
    strncpy(buf, instrs, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';

    int count = 0;
    char *token = strtok(buf, ";");
    while (token && count < max_count) {
        char cmd[20], args[200];

        if (sscanf(token, "%19[^ (](%199[^)])", cmd, args) == 2) {
            if (strcmp(cmd, "DECLARE") == 0) {
                out[count++] = parse_declare(args);
            } else if (strcmp(cmd, "ADD") == 0) {
                out[count++] = parse_add_sub(args, 1);
            } else if (strcmp(cmd, "SUBTRACT") == 0) {
                out[count++] = parse_add_sub(args, 0);
            } else if (strcmp(cmd, "PRINT") == 0) {
                out[count++] = parse_print(args);
            } else if (strcmp(cmd, "SLEEP") == 0) {
                out[count++] = parse_sleep(args);
            } else if (strcmp(cmd, "FOR") == 0) {
                out[count++] = parse_for(args);
            }
            else if (strcmp(cmd, "READ") == 0) {
                out[count++] = parse_read(args);
            } else if (strcmp(cmd, "WRITE") == 0) {
                out[count++] = parse_write(args);
            }
        }
        
        token = strtok(NULL, ";");
    }

    return count;
}

Process *generate_dummy_process(Config config) {
    int min_ins = config.min_ins > 1 ? config.min_ins : 1;
    int max_ins = config.max_ins > min_ins ? config.max_ins : min_ins;
    int num_inst = min_ins + rand() % (max_ins - min_ins + 1);
    int memory_allocation = config.min_mem_per_proc + rand() % (config.max_mem_per_proc - config.min_mem_per_proc + 1);

    Process *p = (Process *)calloc(1, sizeof(Process));
    if (!p) {
        printf("[ERROR] Failed to allocate memory for process!\n");
        return NULL;
    }

    // Use thread-safe PID assignment
    int assigned_pid = ++next_pid;  // Simple increment for now
    
    // *** FIX: Proper string initialization ***
    snprintf(p->name, MAX_PROCESS_NAME - 1, "%d", assigned_pid);
    p->name[MAX_PROCESS_NAME - 1] = '\0';  // Ensure null termination
    
    p->pid = assigned_pid;
    p->state = READY;
    p->program_counter = 0;
    p->num_var = 0;
    p->num_inst = num_inst;
    p->variables_capacity = 8;
    p->in_memory = 0;
    p->for_depth = 0;  // *** FIX: Initialize for_depth ***
    p->ticks_ran_in_quantum = 0;  // *** FIX: Initialize quantum ticks ***
    p->last_exec_time = 0;  // *** FIX: Initialize to 0, will be set when scheduled ***
    p->num_pages = memory_allocation / config.mem_per_frame;
    p->page_table = (PageTableEntry *)calloc(p->num_pages, sizeof(PageTableEntry));

    // *** FIX: Safer memory allocation with error checking ***
    p->variables = (Variable *)calloc(p->variables_capacity, sizeof(Variable));
    if (!p->variables) {
        printf("[ERROR] Failed to allocate variables array!\n");
        free(p);
        return NULL;
    }

    p->instructions = (Instruction *)calloc(num_inst, sizeof(Instruction));
    if (!p->instructions) {
        printf("[ERROR] Failed to allocate instructions array!\n");
        free(p->variables);
        free(p);
        return NULL;
    }

    p->memory_allocation = memory_allocation;

    // Seed random only once
    static int seeded = 0;
    if (!seeded) { 
        srand((unsigned int)time(NULL)); 
        seeded = 1; 
    }

    // Generate random instructions with bounds checking
    for (int i = 0; i < num_inst; i++) {
        int t = rand() % 6; // 0=DECLARE, 1=ADD, 2=SUBTRACT, 3=PRINT, 4=SLEEP, 5=FOR
        char buf[64];
        
        // *** FIX: Ensure we don't access invalid indices ***
        int v2_idx = (i > 0) ? rand() % i : 0;
        int v3_is_var = rand() % 2;
        int v3_idx = (i > 0) ? rand() % i : 0;
        int v3_val = rand() % 100;
        
        char v3_buf[16];
        if (v3_is_var && i > 0) {
            snprintf(v3_buf, sizeof(v3_buf), "v%d", v3_idx);
        } else {
            snprintf(v3_buf, sizeof(v3_buf), "%d", v3_val);
        }

        // *** FIX: Initialize instruction struct to zero ***
        memset(&p->instructions[i], 0, sizeof(Instruction));

        switch (t) {
            case 0: // DECLARE
                snprintf(buf, sizeof(buf), "v%d,%d", i, rand() % 100);
                p->instructions[i] = parse_declare(buf);
                break;
            case 1: // ADD
                snprintf(buf, sizeof(buf), "v%d,v%d,%s", i, v2_idx, v3_buf);
                p->instructions[i] = parse_add_sub(buf, 1);
                break;
            case 2: // SUBTRACT
                snprintf(buf, sizeof(buf), "v%d,v%d,%s", i, v2_idx, v3_buf);
                p->instructions[i] = parse_add_sub(buf, 0);
                break;
            case 3: // PRINT
                snprintf(buf, sizeof(buf), "+v%d", (i > 0) ? rand() % i : 0);
                p->instructions[i] = parse_print(buf);
                break;
            case 4: // SLEEP
                snprintf(buf, sizeof(buf), "%d", 1 + rand() % 10);
                p->instructions[i] = parse_sleep(buf);
                break;
            case 5: // FOR
                snprintf(buf, sizeof(buf), "[v%d,v%d];%d", i, v2_idx, 1 + rand() % 5);
                p->instructions[i] = parse_for(buf);
        }
    }

    return p;
}
// for debugging
void print_process_info(Process *p) {
    printf("Process PID: %d\n", p->pid);
    printf("Process name: %s\n", p->name);
    printf("Number of instructions: %d\n", p->num_inst);
    for (int i = 0; i < p->num_inst; i++) {
        Instruction *inst = &p->instructions[i];
        switch (inst->type) {
            case DECLARE:
                printf("  %2d: DECLARE(%s, %d)\n", i, inst->arg1, inst->value);
                break;
            case ADD:
                printf("  %2d: ADD(%s, %s, %s)\n", i, inst->arg1, inst->arg2, inst->arg3);
                break;
            case SUBTRACT:
                printf("  %2d: SUBTRACT(%s, %s, %s)\n", i, inst->arg1, inst->arg2, inst->arg3);
                break;
            case PRINT:
                printf("  %2d: PRINT(%s)\n", i, inst->arg1);
                break;
            case SLEEP:
                printf("  %2d: SLEEP(%d)\n", i, inst->value);
                break;
            case FOR:
                printf("  %2d: FOR([...], %d)\n", i, inst->repeat_count);
                break;
            case READ:
                printf("  %2d: READ(%s, %s)\n", i, inst->arg1, inst->arg2);
                break;
            case WRITE:
                printf("  %2d: WRITE(%s, %d)\n", i, inst->arg1, inst->value);
                break;
            default:
                printf("  %2d: UNKNOWN\n", i);
        }
    }
}
