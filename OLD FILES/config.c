#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "config.h"

#define yellow "\x1b[33m"
#define reset "\x1b[0m"


Config system_config;

void print_color(const char *color, const char *text);

void load_config() {
    FILE *file = fopen("config.txt", "r");
    if (!file) {
        printf("Error: Could not open config.txt\n");
        return;
    }

    char key[64], value[64];

    while (fscanf(file, "%s %s", key, value) == 2) {
        size_t len = strlen(value);
        if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
            memmove(value, value + 1, len - 2);
            value[len - 2] = '\0';
        }

        if (strcmp(key, "num-cpu") == 0) {
            int val = atoi(value);
            if (val >= 1 && val <= 128) system_config.num_cpu = val;
            else printf("Warning: Invalid value for num-cpu (must be 1–128)\n");

        } else if (strcmp(key, "scheduler") == 0) {
            if (strcmp(value, "fcfs") == 0 || strcmp(value, "rr") == 0)
                strncpy(system_config.scheduler, value, sizeof(system_config.scheduler));
            else
                printf("Warning: scheduler is invalid.\n");

        } else if (strcmp(key, "quantum-cycles") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1) system_config.quantum_cycles = val;
            else print_color(yellow, "Warning: quantum-cycles is invalid.\n");

        } else if (strcmp(key, "batch-process-freq") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1) system_config.batch_process_freq = val;
            else print_color(yellow, "Warning: batch-process-freq is invalid.\n");

        } else if (strcmp(key, "min-ins") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1) system_config.min_ins = val;
            else print_color(yellow, "Warning: min-ins is invalid.\n");

        } else if (strcmp(key, "max-ins") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val >= 1) system_config.max_ins = val;
            else print_color(yellow, "Warning: max-ins is invalid.\n");

        } else if (strcmp(key, "delay-per-exec") == 0 || strcmp(key, "delays-per-exec") == 0) {
            unsigned int val = (unsigned int)strtoul(value, NULL, 10);
            if (val <= UINT_MAX) system_config.delay_per_exec = val;
            else print_color(yellow, "Warning: delay-per-exec is invalid.\n");
        } else if (strcmp(key, "max-overall-mem") == 0){
                system_config.max_overall_mem = atoi(value);
        } else if (strcmp(key, "mem-per-frame") == 0){
                system_config.mem_per_frame = atoi(value);
        } else if (strcmp(key, "mem-per-proc") == 0){
                system_config.mem_per_proc = atoi(value);
        }
    }    

    fclose(file);
}
