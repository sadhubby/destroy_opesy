#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "barebones.h"

#define MAX_VARIABLES 64
#define MAX_INSTRUCTIONS 32

typedef struct {
    char name[16];
    unsigned short value;
} Variable;

static Variable vars[MAX_VARIABLES];
static int var_count = 0;

unsigned short* get_var(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return &vars[i].value;
    }
    if (var_count < MAX_VARIABLES) {
        strncpy(vars[var_count].name, name, sizeof(vars[var_count].name) - 1);
        vars[var_count].name[sizeof(vars[var_count].name) - 1] = '\0';
        vars[var_count].value = 0;
        return &vars[var_count++].value;
    }
    return NULL;
}

char* trim(char* str) {
    while (*str == ' ' || *str == '\t') str++;
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t')) *end-- = '\0';
    return str;
}

void barebones(const char *instr) {
    char safe_instr[512];
    strncpy(safe_instr, instr, sizeof(safe_instr)-1);
    safe_instr[sizeof(safe_instr)-1] = '\0';
    instr = trim(safe_instr);

    if (strncmp(instr, "DECLARE(", 8) == 0) {
        char var[32]; int val;
        sscanf(instr, "DECLARE(%[^,], %d)", var, &val);
        *get_var(trim(var)) = val;

    } else if (strncmp(instr, "ADD(", 4) == 0) {
        char var1[32], var2[32], var3[32];
        if (sscanf(instr, "ADD(%31[^,], %31[^,], %31[^)])", var1, var2, var3) == 3) {
            unsigned short *res = get_var(trim(var1));
            unsigned short *op1 = get_var(trim(var2));
            unsigned short *op2 = get_var(trim(var3));
            *res = *op1 + *op2;
        } else {
            printf(">> ADD parsing error: %s\n", instr);
        }

    } else if (strncmp(instr, "SUBTRACT(", 9) == 0) {
        char var1[32], var2[32], var3[32];
        if (sscanf(instr, "SUBTRACT(%31[^,], %31[^,], %31[^)])", var1, var2, var3) == 3) {
            unsigned short *res = get_var(trim(var1));
            unsigned short *op1 = get_var(trim(var2));
            unsigned short *op2 = get_var(trim(var3));
            *res = *op1 - *op2;
        } else {
            printf(">> SUBTRACT parsing error: %s\n", instr);
        }

    } else if (strncmp(instr, "PRINT(", 6) == 0) {
        char msg[256], var[32];
        if (strstr(instr, "+")) {
            if (sscanf(instr, "PRINT(\"%[^\"]\"+%31[^)])", msg, var) == 2)
                printf(">> %s%d\n", msg, *get_var(trim(var)));
            else
                printf(">> PRINT parsing error: %s\n", instr);
        } else {
            if (sscanf(instr, "PRINT(\"%[^\"]\")", msg) == 1)
                printf(">> %s\n", msg);
            else
                printf(">> PRINT parsing error: %s\n", instr);
        }

    } else if (strncmp(instr, "SLEEP(", 6) == 0) {
        int ticks;
        if (sscanf(instr, "SLEEP(%d)", &ticks) == 1)
            usleep(ticks * 1000);
        else
            printf(">> SLEEP parsing error: %s\n", instr);

    } else if (strncmp(instr, "FOR([", 5) == 0) {
        const char *start = strchr(instr, '[') + 1;
        const char *end = strrchr(instr, ']');
        if (!start || !end || end <= start) {
            printf(">> FOR instruction syntax error: %s\n", instr);
            return;
        }
        char loop_body[512] = {0};
        strncpy(loop_body, start, end-start);
        loop_body[end-start] = '\0';

        // Extract repeats
        int repeats=1;
        if (sscanf(end+2, "%d", &repeats) != 1) {
            printf(">> FOR repeat parsing error: %s\n", instr);
            return;
        }

        // Split instructions carefully
        char *tokens[MAX_INSTRUCTIONS];
        int t=0, depth=0;
        tokens[t++] = loop_body;
        for (int i=0; loop_body[i]; i++) {
            if (loop_body[i] == '[') depth++;
            else if (loop_body[i] == ']') depth--;
            else if (loop_body[i]==',' && depth==0) {
                loop_body[i]='\0';
                if (t < MAX_INSTRUCTIONS) tokens[t++] = &loop_body[i+1];
            }
        }

        for (int r=0; r<repeats; r++) {
            for (int i=0; i<t; i++) {
                barebones(trim(tokens[i]));
            }
        }

    } else {
        printf(">> Unknown instruction: %s\n", instr);
    }
}
