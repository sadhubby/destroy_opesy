#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "instruction.h"

static int random_val(int min, int max){
    return min + rand() % (max - min + 1);
}

static void random_var(char *buf, int idx){
    sprintf(buf, "var%d", idx);
}

static void generate_instructions(char *out, int *ins, int min_ins, int max_ins, int nest, int max_nest, int num_vars, char**var_names, const char *proc_name){

    int n = *ins;
    int max_total = max_ins;
    int min_total = min_ins;

    if (nest == 0) {
        for (int i = 0; i < num_vars; i++) {
            char line[64];
            sprintf(line, "DECLARE(%s, %d)\n", var_names[i], random_val(1, 100));
            strcat(out, line);
            if (strlen(out) + strlen(line) + 1 >= 256000) break;
            
            n++;
        }
    }
        int ops[] = {0, 1, 2, 3, 4, 5}; // 0=PRINT, 1=ADD, 2=SUB, 3=SLEEP, 4=FOR, 5=PRINT+VAR
    int ops_count = (nest < max_nest) ? 6 : 5; // Only allow FOR if not too deep

    while (n < max_total) {
        int op = ops[rand() % ops_count];
        char line[512] = {0};

        if (op == 0) { // PRINT
            sprintf(line, "PRINT(\"Hello world from %s!\")\n", proc_name);
            n++;
        } else if (op == 1) { // ADD
            int v1 = rand() % num_vars, v2 = rand() % num_vars, v3 = rand() % num_vars;
            sprintf(line, "ADD(%s, %s, %s)\n", var_names[v1], var_names[v2], var_names[v3]);
            n++;
        } else if (op == 2) { // SUBTRACT
            int v1 = rand() % num_vars, v2 = rand() % num_vars, v3 = rand() % num_vars;
            sprintf(line, "SUBTRACT(%s, %s, %s)\n", var_names[v1], var_names[v2], var_names[v3]);
            n++;
        } else if (op == 3) { // SLEEP
            sprintf(line, "SLEEP(%d)\n", random_val(10, 100));
            n++;
        } else if (op == 4 && nest < max_nest) { // FOR loop
            int body_ins = random_val(1, 3);
            char body[1024] = {0};
            int before = n;
            int n_before = n;
            generate_instructions(body, &n, body_ins, body_ins, nest+1, max_nest, num_vars, var_names, proc_name);
            int repeats = random_val(2, 4);
            if (strlen(body) > 0 && n > n_before) { // Only emit FOR if body is not empty
                sprintf(line, "FOR([%s], %d)\n", body, repeats);
                n++; // count the FOR as one instruction
            } else {
                continue; // skip this FOR, try another instruction
            }
        } else if (op == 5) { // PRINT with variable
            int v = rand() % num_vars;
            sprintf(line, "PRINT(\"Hello world from %s!\"+%s)\n", proc_name, var_names[v]);
            n++;
        }

        if (n <= max_total)
            strcat(out, line);
            if (strlen(out) + strlen(line) + 1 >= 256000) break;

       // Randomly stop if min reached
    }

    *ins = n;
}

// Main function to generate a process's instruction list
void generate_process_instructions(char *out, int min_ins, int max_ins, const char *proc_name) {
    int num_vars = random_val(2, 8 + rand() % 8); // 2-15 variables
    char **var_names = malloc(num_vars * sizeof(char*));
    for (int i = 0; i < num_vars; i++) {
        var_names[i] = malloc(8);
        random_var(var_names[i], i);
    }

    int ins_count = 0;
    out[0] = '\0';

    // THIS IS THE IMPORTANT LINE:
    int total_instructions = random_val(min_ins, max_ins);

    generate_instructions(out, &ins_count, total_instructions, total_instructions, 0, 3, num_vars, var_names, proc_name);

    for (int i = 0; i < num_vars; i++) free(var_names[i]);
    free(var_names);
}