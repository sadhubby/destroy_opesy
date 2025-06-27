#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "barebones.h"

#define MAX_VARIABLES 64
#define MAX_FOR_DEPTH 3
#define MAX_INSTRUCTIONS 16
typedef struct {
    char name[16];
    unsigned short value;
} Variable;

static Variable vars[MAX_VARIABLES];
static int var_count = 0;

unsigned short* get_var (const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return &vars[i].value;
    }
    if (var_count <MAX_VARIABLES) {
        strncpy(vars[var_count].name, name, sizeof(vars[var_count].name) - 1);
        vars[var_count].name[sizeof(vars[var_count].name) - 1] = '\0';
        vars[var_count].value = 0;
        return &vars[var_count++].value;
    }
    return NULL;
}


char* trim(char* str) {
    while (*str == ' '  || *str == '\t') str++;
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t')) *end-- = '\0';
    return str;
}

void barebones (const char *instr) {
    char safe_instr[256];
    strncpy(safe_instr, instr, sizeof(safe_instr) - 1);
    safe_instr[sizeof(safe_instr) - 1] = '\0';
    instr = trim(safe_instr);

    if (strncmp(instr, "DECLARE(", 8) == 0) {
        char var[16]; int val;
        sscanf(instr, "DECLARE(%[^,], %d)", var, &val);
        *get_var(trim(var)) = val;

    } else if (strncmp(instr, "ADD(", 4) == 0) {
        const char *args_start = strchr(instr, '(');
        const char *args_end = strrchr(instr, ')');
        if (!args_start || !args_end || args_end <= args_start + 1) {
            printf("ADD instruction.\n ");
            return;
        }

        char inside[128];
        snprintf(inside, sizeof(inside), " %.*s", (int)(args_end - args_start - 1), args_start + 1);
        char var1[32], var2[32], var3[32];
        if (sscanf(inside, "%31[^,], %31[^,], %31[^,] ", var1, var2, var3) != 3) {
            printf("ADD instruction.\n");
            return;
        }

        unsigned short *res =get_var(trim(var1));
        unsigned short *op1 =get_var(trim(var2));
        unsigned short *op2 =get_var(trim(var3));
        *res = *op1 + *op2;

    } else if (strncmp(instr, "S UBTRACT(", 9) == 0) {
        const char *args_start = strchr(instr, '(');
        const char *args_end = strrchr(instr, ')');
        if (!args_start || !args_end || args_end <= args_start + 1) {
            printf("SUBTRACT instruction.\n");
            return;
        }

        char inside[128];
        snprintf(inside, sizeof(inside),"%.*s", (int)(args_end - args_start - 1), args_start + 1);
        char var1[32], var2[32], var3[32];
        if (sscanf(inside, " %31[^,], %31[^,], %31[^,] ",var1, var2, var3) != 3) {
            printf("SUBTRACT instruction.\n");
            return;
        }

        unsigned short *res =get_var(trim(var1));
        unsigned short *op1 =get_var(trim(var2));
        unsigned short *op2 =get_var(trim(var3));
        *res = *op1 - *op2;


    } else if (strncmp(instr, "PRINT(", 6) == 0) {
        char msg[128], var[16];
        if (strstr(instr, "+")){
            sscanf(instr, "PRINT(\"%[^\"]\"+%[^)])", msg, var);
            printf(">> %s%d\n", msg, *get_var(trim(var)));
        } else {
            sscanf(instr, "PRINT(\"%[^\"]\")", msg);
            printf(">> %s\n", msg);
        }

    } else if (strncmp(instr,"SLEEP(", 6)== 0)  {
        int ticks;
        sscanf(instr, "SLEEP(%d)", &ticks);
        usleep(ticks * 1000);

    } else if (strncmp(instr,"FOR([", 5) == 0) {
        const char *start = strchr(instr, '[') + 1;
        const char *end = strrchr(instr, ']');
        int repeat = 1;
        sscanf(end + 2, "%d", &repeat);

        char loop_block[512] = {0};
        snprintf(loop_block, sizeof(loop_block),"%.*s", (int)(end - start), start);

        char *tokens[MAX_INSTRUCTIONS];
        int depth = 0, t = 0;
        tokens[t++] =loop_block;

        for (int i = 0; loop_block[i]; i++) {
            if (loop_block[i] == '[') depth++;
            if (loop_block[i] == ']') depth--;
            if (loop_block[i] == ',' && depth == 0) {
                loop_block[i] = '\0';
                if (t < MAX_INSTRUCTIONS) {
                    tokens[t++] = &loop_block[i + 1];
                } else {
                    printf("Too many instructions in FOR loop\n");
                    return;
                }
            }
        }

        for (int r = 0; r < repeat; r++) {
            for (int i = 0; i < t; i++) {
                barebones(trim(tokens[i]));
            }
        }

    } else {
        printf(">> Unknown instruction: %s\n", instr);
    }
}
