#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

Config system_config;

void load_config() {
    FILE *file = fopen("config.txt", "r");
    if (!file) {
        printf("Error: Could not open config.txt\n");
        return;
    }

    char key[64], value[64];

    while (fscanf(file, "%s %s", key, value) == 2) {
        if (strcmp(key, "num-cpu") == 0)
            system_config.num_cpu = atoi(value);
        else if (strcmp(key, "scheduler") == 0)
            strncpy(system_config.scheduler, value, sizeof(system_config.scheduler));
            size_t len = strlen(system_config.scheduler);
            if (system_config.scheduler[0] == '"' && system_config.scheduler[len-1] == '"') {
                memmove(system_config.scheduler, system_config.scheduler+1, len-2);
                system_config.scheduler[len-2] = '\0';
            }
        else if (strcmp(key, "quantum-cycles") == 0)
            system_config.quantum_cycles = atoi(value);
        else if (strcmp(key, "batch-process-freq") == 0)
            system_config.batch_process_freq = atoi(value);
        else if (strcmp(key, "min-ins") == 0)
            system_config.min_ins = atoi(value);
        else if (strcmp(key, "max-ins") == 0)
            system_config.max_ins = atoi(value);
        else if (strcmp(key, "delay-per-exec") == 0)
            system_config.delay_per_exec = atoi(value);
    }

    fclose(file);
}
