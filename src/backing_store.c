#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "process.h"

#define BACKING_STORE_FILENAME "csopesy-backing-store.txt"

void write_process_to_backing_store(Process *p){
    FILE *fp = fopen(BACKING_STORE_FILENAME, "ab");
    if(!fp) return;
    fwrite(p, sizeof(Process), 1, fp);
    fclose(fp);
}

int read_all_processes_from_backing_store(Process ***out_array){
    FILE *fp = fopen(BACKING_STORE_FILENAME, "rb");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    int count = filesize / sizeof (Process);
    rewind(fp);

    Process **arr = malloc(sizeof(Process *) * count);
    for(int i = 0; i < count; i++){
        arr[i] = malloc(sizeof(Process));
        fread(arr[i], sizeof(Process), 1, fp);
    }
    fclose(fp);
    *out_array = arr;
    return count;
}

void remove_process_from_backing_store(int pid){
    Process **arr = NULL;
    int count = read_all_processes_from_backing_store(&arr);
    if(count == 0) return;

    FILE *fp = fopen(BACKING_STORE_FILENAME, "wb");
    if (!fp){
        for(int i = 0; i < count; i++){
            free(arr[i]);
            
        }
        free(arr);
        return;
    }
    for (int i = 0; i < count; i++){
        if(arr[i]->pid != pid){
            fwrite(arr[i], sizeof(Process), 1, fp);
        }
        free(arr[i]);
    }
    free(arr);
    fclose(fp);
}

void print_backing_store_contents() {
    Process **arr = NULL;
    int count = read_all_processes_from_backing_store(&arr);
    printf("\nBacking store contains %d process(es):\n", count);
    for (int i = 0; i < count; i++) {
        printf("PID: %d, Name: %s, State: %d\n", arr[i]->pid, arr[i]->name, arr[i]->state);
        free(arr[i]);
    }
    free(arr);
    if (count == 0) {
        printf("(Backing store is empty)\n");
    }
}