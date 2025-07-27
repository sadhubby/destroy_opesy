#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "config.h"

// colors for style
#define yellow "\x1b[33m"
#define reset "\x1b[0m"
void printColor(const char *color, const char *text);

int load_config(Config *config) {
    FILE *file = fopen("config.txt", "r");
    if (!file) {
        perror("Error: Could not open config.txt");
        return 0;
    }

    char key[64], value[64];

    while (fscanf(file, "%s %s", key, value) == 2) {
        // Strip surrounding quotes from value
        size_t len = strlen(value);
        if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
            memmove(value, value + 1, len - 2);
            value[len - 2] = '\0';
        }

        // Parse each config field with error check-  num-cpu
        if (strcmp(key, "num-cpu") == 0) {
            int val = atoi(value);
            if (val >= 1 && val <= 128)
                config->num_cpu = val;
            else
                printColor(yellow, "Warning: Invalid value for num-cpu (must be 1–128)\n");

        // scheduler
        } else if (strcmp(key, "scheduler") == 0) {
            if (strcmp(value, "fcfs") == 0 || strcmp(value, "rr") == 0)
                strncpy(config->scheduler, value, sizeof(config->scheduler) - 1);
            else
                printColor(yellow, "Warning: scheduler is invalid. Must be 'fcfs' or 'rr'\n");

        // quantum cycles
        } else if (strcmp(key, "quantum-cycles") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1)
                config->quantum_cycles = val;
            else
                printColor(yellow, "Warning: quantum-cycles is invalid (must be ≥ 1)\n");

        // batch process freq
        } else if (strcmp(key, "batch-process-freq") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1)
                config->batch_process_freq = val;
            else
                printColor(yellow, "Warning: batch-process-freq is invalid (must be ≥ 1)\n");

        // min-ins
        } else if (strcmp(key, "min-ins") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1)
                config->min_ins = val;
            else
                printColor(yellow, "Warning: min-ins is invalid (must be ≥ 1)\n");
        
        // max-ins
        } else if (strcmp(key, "max-ins") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1)
                config->max_ins = val;
            else
                printColor(yellow, "Warning: max-ins is invalid (must be ≥ 1)\n");
        

        // delay per exec
        } else if (strcmp(key, "delay-per-exec") == 0 || strcmp(key, "delays-per-exec") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 0)
                config->delay_per_exec = val;
            else
                printColor(yellow, "Warning: max-ins is invalid (must be ≥ 0)\n");
        }  
        
        //memory related variables
        else if (strcmp(key, "max-overall-mem") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 0)
                config->max_overall_mem = val; //  change this to max_overall_mem = val from config file
        }
        else if (strcmp(key, "mem-per-frame") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 0)
                config->mem_per_frame = val;
        }
        else if (strcmp(key, "min-mem-per-proc") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 0)
                config->min_mem_per_proc= val;
        }
        else if (strcmp(key, "max-mem-per-proc") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 0)
                config->max_mem_per_proc= val;
        }


        else {
            printf("Warning: Unrecognized config key: %s\n", key);
        }
    }

    fclose(file);
    return 1;
}