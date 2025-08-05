#include "process.h"
#include "scheduler.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// retain in uint16 bounds
#define CLAMP_UINT16(x) ((x) > 65535 ? 65535 : (x))

// get variable given a process and a variable name
Variable *get_variable(Process *p, const char *name) {
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

    // Grow variable array if needed
    if (p->num_var >= p->variables_capacity) {
        int new_cap = p->variables_capacity * 2;
        Variable *new_vars = realloc(p->variables, new_cap * sizeof(Variable));
        if (!new_vars) {
            printf("[ERROR] Failed to realloc variables array for process %d!\n", p->pid);
            return NULL;
        }
        p->variables = new_vars;
        p->variables_capacity = new_cap;
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
    if (!p) {
        printf("[ERROR] NULL process passed to execute_instruction\n");
        return;
    }

    // Validate process state
    if (!p->instructions || !p->variables || !p->page_table) {
        printf("[ERROR] Process %d has corrupted state (instructions=%p, variables=%p, page_table=%p)\n",
               p->pid, (void*)p->instructions, (void*)p->variables, (void*)p->page_table);
        p->state = FINISHED;
        return;
    }

    // Validate memory configuration
    if (p->num_pages == 0 || p->memory_allocation == 0 || p->memory_allocation > config.max_mem_per_proc) {
        printf("[ERROR] Process %d has invalid memory configuration: pages=%u, allocation=%u\n",
               p->pid, p->num_pages, p->memory_allocation);
        p->state = FINISHED;
        return;
    }

    Instruction *inst;

    //guard for out of bounds
    if (p->program_counter >= p->num_inst) {
        // Already finished, do nothing
        return;
    }
    // if inside a for loop
    // if (p->for_depth > 0) {
    //     ForContext *ctx = &p->for_stack[p->for_depth - 1];
    //     if (ctx->current_index < ctx->sub_instruction_count) {
    //         inst = &ctx->sub_instructions[ctx->current_index];
    //     } else if (ctx->remaining > 0) {
    //         ctx->remaining--;
    //         ctx->current_index = 0;
    //         inst = &ctx->sub_instructions[ctx->current_index];
    //     } else {
    //         // when the loop is done, pop from stack
    //         p->for_depth--;
    //         p->program_counter++;
    //         return;
    //     }
    // }
    // else
    inst = &p->instructions[p->program_counter];

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
            break;
        }
        
        // for
        case FOR: {
            // ForContext *ctx = &p->for_stack[p->for_depth++];
            // ctx->repeat_count = inst->repeat_count;
            // ctx->remaining = inst->repeat_count - 1;
            // ctx->current_index = 0;
            // ctx->sub_instructions = inst->sub_instructions;
            // ctx->sub_instruction_count = inst->sub_instruction_count;
            // inst = &ctx->sub_instructions[ctx->current_index];
            // execute_instruction(p);
            // break;
        }
        // sleep
        case SLEEP: {
            // sleep for x ticks
            p->state = SLEEPING;
            p->sleep_until_tick = CPU_TICKS + inst->value;
        }
        // read
        case READ: {
            int success = 1;  // Track if instruction succeeds

            // Validate and clean variable name
            char valid_name[50] = {0};
            if (!inst->arg1[0]) {
                printf("[DEBUG] Raw arg1 empty in READ for P%d\n", p->pid);
                p->program_counter++;  // Make sure to increment PC before returning
                return;   // Exit immediately on empty arg1
            } 
            
            // Only continue if we have a non-empty arg1
            {
                // Make a copy for validation
                strncpy(valid_name, inst->arg1, sizeof(valid_name) - 1);
                valid_name[sizeof(valid_name) - 1] = '\0';
                
                // First pass: count non-whitespace characters
                int valid_chars = 0;
                for (const char *p = valid_name; *p; p++) {
                    if (!isspace((unsigned char)*p)) {
                        valid_chars++;
                    }
                }
                
                if (valid_chars == 0) {
                    printf("[ERROR] Variable name contains only whitespace in READ for P%d\n", p->pid);
                    success = 0;
                } else {
                    // Second pass: remove whitespace
                    int j = 0;
                    for (int i = 0; valid_name[i]; i++) {
                        if (!isspace((unsigned char)valid_name[i])) {
                            valid_name[j++] = valid_name[i];
                        }
                    }
                    valid_name[j] = '\0';
                }

                // Validate cleaned variable name
                if (!valid_name[0]) {
                    printf("[ERROR] Missing variable name in READ for P%d\n", p->pid);
                    success = 0;
                } else {
                    // Check for valid characters
                    for (const char *c = valid_name; *c; c++) {
                        if (!isalnum((unsigned char)*c) && *c != '_') {
                            printf("[ERROR] Invalid variable name '%s' in READ for P%d\n", 
                                   valid_name, p->pid);
                            success = 0;
                            break;
                        }
                    }

                    if (success) {
                        // Pre-create/validate the variable
                        Variable *v = get_variable(p, valid_name);
                        if (!v) {
                            printf("[ERROR] Failed to get/create variable %s for P%d\n", 
                                   valid_name, p->pid);
                            success = 0;
                        } else {
                            // Copy cleaned name to instruction
                            strncpy(inst->arg1, valid_name, sizeof(inst->arg1) - 1);
                            inst->arg1[sizeof(inst->arg1) - 1] = '\0';
                        }
                    }
                }
            }

            // Only proceed with address parsing if variable name was valid
            if (success) {
                // Validate and parse address
                if (!inst->arg2[0]) {
                    printf("[ERROR] Missing address in READ for P%d\n", p->pid);
                    p->program_counter++;  // Make sure to increment PC before returning
                    return;   // Exit immediately on empty arg2
                }

                uint32_t address = 0;
                const char *addr_str = inst->arg2;
                
                // Handle 0x prefix
                if (strncmp(addr_str, "0x", 2) == 0) {
                    addr_str += 2;
                }

                // Parse hex address
                char *end;
                address = strtoul(addr_str, &end, 16);
                if (*end != '\0' || end == addr_str) {
                    printf("[ERROR] Invalid hex address '%s' in READ for P%d\n", 
                           inst->arg2, p->pid);
                    success = 0;
                } else {
                    // Validate address alignment
                    if (address % 2 != 0) {
                        printf("[ERROR] Unaligned memory access at 0x%X for P%d\n", 
                               address, p->pid);
                        success = 0;
                    }
                    // Validate address is within process's memory allocation
                    else if (address >= p->memory_allocation) {
                        printf("[ERROR] Address 0x%X exceeds process memory allocation %u for P%d\n",
                               address, p->memory_allocation, p->pid);
                        success = 0;
                    }
                    else {
                        // Attempt the memory read
                        int read_success = 0;
                        int max_attempts = 3;  // Prevent infinite loops
                        uint16_t value;

                        for (int attempt = 0; attempt < max_attempts; attempt++) {
                            value = memory_read(address, p, &read_success);
                            if (read_success) break;
                            
                            if (attempt < max_attempts - 1) {
                                printf("[DEBUG] Memory read attempt %d failed, retrying...\n", attempt + 1);
                            }
                        }

                        if (!read_success) {
                            printf("[ERROR] Memory read failed for P%d at address 0x%X after %d attempts\n", 
                                   p->pid, address, max_attempts);
                            success = 0;
                        } else {
                            // Store the read value in our validated variable
                            Variable *v = get_variable(p, inst->arg1);
                            if (v) {
                                v->value = value;
                                printf("[DEBUG] Successfully read value %u from address 0x%X for P%d\n",
                                       value, address, p->pid);
                            } else {
                                printf("[ERROR] Variable became invalid during READ for P%d\n", p->pid);
                                success = 0;
                            }
                        }
                    }
                }
            }

            // Always increment PC for READ instructions, whether successful or not
            p->program_counter++;
            printf("[DEBUG] P%d PC incremented to %d after READ instruction\n", 
                   p->pid, p->program_counter);
            break;
        }
        // // writee
        // case WRITE: {
        //     uint32_t address = 0;
        //     sscanf(inst->arg1, "%x", &address);
        //     uint16_t value = CLAMP_UINT16(inst->value);
        //     memory_write(address, value, p);
        //     break;
        // }

    }

    // Only increment PC if instruction executed successfully
    // SLEEP doesn't increment since it needs to stay on same instruction
    // READ increments on its own for error cases
    if (inst->type != SLEEP && inst->type != READ) {
        p->program_counter++;
    }

    // Record the time of this instruction execution
    if (p->state != SLEEPING) {
        p->last_exec_time = time(NULL);
    }

    return;
}

Process **process_table = NULL;
uint32_t num_processes = 0;
uint32_t process_table_size = 0;

void add_process(Process *p) {
    if (!p) {
        printf("[ERROR] Attempted to add NULL process to process table\n");
        return;
    }

    if (num_processes >= process_table_size) {
        uint32_t new_size = process_table_size == 0 ? 8 : process_table_size * 2;
        Process **new_table = realloc(process_table, new_size * sizeof(Process *));
        if (!new_table) {
            printf("[ERROR] Failed to resize process table from %u to %u\n", 
                   process_table_size, new_size);
            return;
        }
        process_table = new_table;
        process_table_size = new_size;
    }

    process_table[num_processes++] = p;
    printf("[DEBUG] Added P%d to process table (slot %u), pages=%u, allocation=%u\n",
           p->pid, num_processes-1, p->num_pages, p->memory_allocation);
}

// trim whitespace
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
    char var[50] = {0};
    char addr[50] = {0};
    char format[50] = {0};
    
    // Skip any leading whitespace
    while (*args && isspace((unsigned char)*args)) args++;
    
    // Validate input string isn't empty
    if (!*args) {
        printf("[ERROR] Empty READ instruction arguments\n");
        return inst;
    }
    
    // Parse with exact format requirements and handle escaped characters
    char *working_copy = strdup(args);
    if (!working_copy) {
        printf("[ERROR] Memory allocation failed in parse_read\n");
        return inst;
    }

    // Clean up the input string first
    char *clean = working_copy;
    char *write = clean;
    
    // Skip leading whitespace
    while (*clean && isspace((unsigned char)*clean)) clean++;
    
    // Copy while removing extra whitespace
    int in_space = 0;
    while (*clean) {
        if (isspace((unsigned char)*clean)) {
            if (!in_space) {
                *write++ = ' ';
                in_space = 1;
            }
        } else {
            *write++ = *clean;
            in_space = 0;
        }
        clean++;
    }
    *write = '\0';
    
    // Remove trailing whitespace
    while (write > working_copy && isspace((unsigned char)*(write-1))) {
        *--write = '\0';
    }

    // Now try to parse the cleaned string
    char *comma = strchr(working_copy, ',');
    if (!comma) {
        printf("[ERROR] Missing comma in READ instruction: %s\n", args);
        free(working_copy);
        return inst;
    }

    // Split at comma
    *comma = '\0';
    char *var_part = working_copy;
    char *addr_part = comma + 1;
    
    // Remove whitespace around both parts
    trim(var_part);
    trim(addr_part);
    
    if (!*var_part || !*addr_part) {
        printf("[ERROR] Missing variable or address in READ: %s\n", args);
        free(working_copy);
        return inst;
    }

    // Copy to our buffers
    strncpy(var, var_part, sizeof(var) - 1);
    strncpy(addr, addr_part, sizeof(addr) - 1);
    var[sizeof(var) - 1] = '\0';
    addr[sizeof(addr) - 1] = '\0';
    
    free(working_copy);

    // Clean up the strings
    trim(var); 
    trim(addr);

    // Validate variable name isn't empty and contains only valid characters
    if (var[0] == '\0') {
        printf("[ERROR] Empty variable name in READ instruction\n");
        return inst;
    }
    
    for (char *p = var; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') {
            printf("[ERROR] Invalid character in variable name: %s\n", var);
            return inst;
        }
    }

    // Validate and normalize address format
    if (strncmp(addr, "0x", 2) != 0) {
        // Try to add 0x prefix if missing
        memmove(addr + 2, addr, strlen(addr) + 1);
        addr[0] = '0';
        addr[1] = 'x';
    }

    // Try parsing the address to validate it
    unsigned int test_addr;
    if (sscanf(addr + 2, "%x", &test_addr) != 1) {
        printf("[ERROR] Invalid hexadecimal address: %s\n", addr);
        return inst;
    }

    strncpy(inst.arg1, var, sizeof(inst.arg1) - 1);
    inst.arg1[sizeof(inst.arg1) - 1] = '\0';
    strncpy(inst.arg2, addr, sizeof(inst.arg2) - 1);
    inst.arg2[sizeof(inst.arg2) - 1] = '\0';
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

static int dummy_pid = 1;

// Generate a dummy process for testing or batch creation
// Process *generate_dummy_process(Config config) {
//     int min_ins = config.min_ins > 1 ? config.min_ins : 1;
//     int max_ins = config.max_ins > min_ins ? config.max_ins : min_ins /* + 1 */; //+1 was commented
//     int num_inst = min_ins + rand() % (max_ins - min_ins + 1);
//     int memory_allocation = config.min_mem_per_proc + rand() % (config.max_mem_per_proc - config.min_mem_per_proc + 1);

//     Process *p = (Process *)calloc(1, sizeof(Process));
//     snprintf(p->name, MAX_PROCESS_NAME, "%d", dummy_pid);
//     p->pid = dummy_pid++;
//     p->state = READY;
//     p->program_counter = 0;
//     p->num_var = 0;
//     p->num_inst = num_inst;
//     p->variables_capacity = 8;
//     p->variables = (Variable *)calloc(p->variables_capacity, sizeof(Variable));
//     p->instructions = (Instruction *)calloc(num_inst, sizeof(Instruction));
//     p->memory_allocation = memory_allocation;
//     p->in_memory = 0;

//     // Seed random
//     static int seeded = 0;
//     if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }

//     // Generate random instructions
//     for (int i = 0; i < num_inst; i++) {
//         int t = rand() % 5; // 0=DECLARE, 1=ADD, 2=SUBTRACT, 3=PRINT, 4=SLEEP
//         char buf[64];
//         int v2_idx = (i > 0) ? rand() % i : 0;
//         int v3_is_var = rand() % 2;
//         int v3_idx = (i > 0) ? rand() % i : 0;
//         int v3_val = rand() % 100;
        
//         // ensure third argument for add and subtract
//         char v3_buf[16];
//         if (v3_is_var && i > 0) {
//             snprintf(v3_buf, sizeof(v3_buf), "v%d", v3_idx);
//         } else {
//             snprintf(v3_buf, sizeof(v3_buf), "%d", v3_val);
//         }

//         // parse respective operations
//         switch (t) {
//             case 0: // DECLARE
//                 snprintf(buf, sizeof(buf), "v%d,%d", i, rand() % 100);
//                 p->instructions[i] = parse_declare(buf);
//                 break;
//             case 1: // ADD
//                 snprintf(buf, sizeof(buf), "v%d,v%d,%s", i, v2_idx, v3_buf);
//                 p->instructions[i] = parse_add_sub(buf, 1);
//                 break;
//             case 2: // SUBTRACT
//                 snprintf(buf, sizeof(buf), "v%d,v%d,%s", i, v2_idx, v3_buf);
//                 p->instructions[i] = parse_add_sub(buf, 0);
//                 break;
//             case 3: // PRINT
//                 snprintf(buf, sizeof(buf), "+v%d", (i > 0) ? rand() % i : 0);
//                 p->instructions[i] = parse_print(buf);
//                 break;
//             case 4: // SLEEP
//                 snprintf(buf, sizeof(buf), "%d", 1 + rand() % 10);
//                 p->instructions[i] = parse_sleep(buf);
//                 break;
//         }
//     }

//     return p;
// }
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

    // *** FIX: Proper string initialization ***
    snprintf(p->name, MAX_PROCESS_NAME - 1, "%d", dummy_pid);
    p->name[MAX_PROCESS_NAME - 1] = '\0';  // Ensure null termination
    
    p->pid = dummy_pid++;
    p->state = READY;
    p->program_counter = 0;
    p->num_var = 0;
    p->num_inst = num_inst;
    p->variables_capacity = 8;
    p->in_memory = 0;
    p->for_depth = 0;  // *** FIX: Initialize for_depth ***
    p->ticks_ran_in_quantum = 0;  // *** FIX: Initialize quantum ticks ***
    p->last_exec_time = 0;  // *** FIX: Initialize to 0, will be set when scheduled ***
    if (config.mem_per_frame == 0) {
        printf("[FATAL] Invalid frame size of 0 in process creation\n");
        free(p);
        return NULL;
    }

    p->num_pages = (memory_allocation + config.mem_per_frame - 1) / config.mem_per_frame;
    printf("[DEBUG-INIT] Creating P%d with allocation=%d, frame_size=%llu, num_pages=%d\n",
           p->pid, memory_allocation, config.mem_per_frame, p->num_pages);

    if (p->num_pages == 0) {
        printf("[ERROR] Process created with 0 pages (allocation=%d, frame_size=%llu)\n",
               memory_allocation, config.mem_per_frame);
        free(p);
        return NULL;
    }

    p->page_table = (PageTableEntry *)calloc(p->num_pages, sizeof(PageTableEntry));
    if (!p->page_table) {
        printf("[ERROR] Failed to allocate page table for P%d (%d pages)\n", 
               p->pid, p->num_pages);
        free(p->variables);
        free(p->instructions);
        free(p);
        return NULL;
    }

    // Initialize all page table entries as invalid
    for (uint32_t i = 0; i < p->num_pages; i++) {
        p->page_table[i].valid = false;
        p->page_table[i].frame_number = -1;
    }

    p->mem_base = 0;  // Initialize to 0, will be set properly when memory is allocated
    p->mem_limit = memory_allocation;

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
        int t = rand() % 6; // 0=DECLARE, 1=ADD, 2=SUBTRACT, 3=PRINT, 4=SLEEP, 5=READ
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
            case 5: // READ
                // First few instructions should be DECLARE to ensure we have variables
                if (i < 3) {
                    snprintf(buf, sizeof(buf), "v%d,%d", i, rand() % 100);
                    p->instructions[i] = parse_declare(buf);
                } else {
                    // First ensure we have a valid variable name
                    char varname[16];
                    snprintf(varname, sizeof(varname), "v%d", i);

                    // Generate a safe virtual address within the process's memory space
                    uint32_t max_pages = p->num_pages > 0 ? p->num_pages - 1 : 0;
                    uint32_t page = rand() % (max_pages + 1);
                    uint32_t max_offset = config.mem_per_frame - 2; // -2 to ensure word alignment
                    uint32_t offset = (rand() % (max_offset / 2)) * 2; // Ensure word alignment
                    
                    // Virtual address is relative to process memory space
                    uint32_t addr = (page * config.mem_per_frame) + offset;
                    
                    // Double-check all bounds
                    if (addr >= p->memory_allocation || offset >= config.mem_per_frame) {
                        // Fallback to DECLARE if address would be invalid
                        snprintf(buf, sizeof(buf), "%s,%d", varname, rand() % 100);
                        p->instructions[i] = parse_declare(buf);
                    } else {
                        // Format with exact spacing and ensure variable name is valid
                        snprintf(buf, sizeof(buf), "%s, 0x%08x", varname, addr);
                        Instruction read_inst = parse_read(buf);
                        
                        // Verify the instruction was parsed correctly
                        if (read_inst.arg1[0] == '\0' || read_inst.arg2[0] == '\0') {
                            // Fallback to DECLARE if READ parsing failed
                            snprintf(buf, sizeof(buf), "%s,%d", varname, rand() % 100);
                            p->instructions[i] = parse_declare(buf);
                        } else {
                            p->instructions[i] = read_inst;
                        }
                    }
                }
                break;
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
