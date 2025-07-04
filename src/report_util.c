// #include <stdio.h>
// #include <time.h>
// #include <string.h>
// #include "scheduler.h"
// #include "config.h"
// #include "report_util.h"

// void write_report_to_file(const char *filename) {
//     FILE *fp = fopen(filename, "w");
//     if (!fp) {
//         perror("Failed to open report file");
//         return;
//     }

//     int cores_used = system_config.num_cpu;

//     fprintf(fp, "CPU Utilization: 100%%\n");
//     fprintf(fp, "Cores used: %d\n", cores_used);
//     fprintf(fp, "Cores available: 0\n\n");

//     fprintf(fp, "-------------------------------------\n");
//     fprintf(fp, "Running processes:\n");

//     for (int i = 0; i < MAX_PROCESSES; i++) {
//         Process *p = &process_list[i];
//         if (p->burst_time == 0) continue;
//         if (p->core_assigned == -1 || p->is_finished) continue;

//         char timestamp[64] = "-";
//         if (p->start_time != 0) {
//             struct tm *ts = localtime(&p->start_time);
//             strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", ts);
//         }

//         fprintf(fp, "%-10s (%s)  Core: %-2d  %4d / %-4d\n",
//                 p->name,
//                 timestamp,
//                 p->core_assigned,
//                 p->finished_print,
//                 p->burst_time);
//     }

//     fprintf(fp, "\nFinished processes:\n");

//     for (int i = 0; i < finished_count; i++) {
//         Process *p = &finished_list[i];
//         char timestamp[64] = "-";
//         if (p->end_time != 0) {
//             struct tm *te = localtime(&p->end_time);
//             strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", te);
//         }

//         fprintf(fp, "%-10s (%s)  Finished   %4d / %-4d\n",
//                 p->name,
//                 timestamp,
//                 p->finished_print,
//                 p->burst_time);
//     }

//     fprintf(fp, "-------------------------------------\n");
//     fprintf(fp, "\nReport generated at: %s\n", filename);

//     fclose(fp);
// }