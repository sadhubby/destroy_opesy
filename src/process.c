// #include <stdlib.h>
// #include <string.h>
// #include <time.h>
// #include "process.h"
// #include "cli.c"
// // #include "instruction.h"

// // constructor
// Process *create_process(const char *name, int pid) {
//     Process *p = malloc(sizeof(Process));
//     if (!p) return NULL;
//     strncpy(p->name, name, 50);
//     p->name[50] = '\0';
//     p->pid = pid;
//     p->finished_print = 0;
//     p->total_prints = config.min_ins + rand() % (config.max_ins - config.min_ins + 1);
//     p->is_finished = 0;

//     p->instructions = malloc(sizeof(Instruction) * p->total_prints);

//     for (int i = 0; i < p->total_prints; i++) {
//         p->instructions[i] = generate_random_instruction();
//     }

//     return p;
// }