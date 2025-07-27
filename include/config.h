#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int num_cpu;
    char scheduler[8];
    int quantum_cycles;
    int batch_process_freq;
    int min_ins;
    int max_ins;
    int delay_per_exec;
    int max_overall_mem;
    int mem_per_frame; 
    int mem_per_proc; 

    double cpu_util;
} Config;

extern Config system_config;

int load_config(Config *config);

#endif
