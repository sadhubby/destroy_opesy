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
} Config;

extern Config system_config;

int load_config(Config *config);

#endif
