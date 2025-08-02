#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "process.h"
#include "backing_store.h"

#define BACKING_STORE_FILENAME "csopesy-backing-store.txt"

// Writes a process and its associated data to the backing store.
void write_process_to_backing_store(Process *p) {
    if (!p) return;
    FILE *fp = fopen(BACKING_STORE_FILENAME, "ab"); // Append Binary
    if (!fp) {
        perror("Failed to open backing store for writing");
        return;
    }

    // 1. Write the main Process struct (without its pointer data)
    fwrite(p, sizeof(Process), 1, fp);
    // 2. Write the instructions array
    if (p->num_inst > 0 && p->instructions) {
        fwrite(p->instructions, sizeof(Instruction), p->num_inst, fp);
    }
    // 3. Write the variables array
    if (p->num_var > 0 && p->variables) {
        fwrite(p->variables, sizeof(Variable), p->num_var, fp);
    }

    fclose(fp);
    // printf("[DEBUG] Process %s (PID: %d) moved to backing store.\n", p->name, p->pid);
}

// Reads the FIRST process from the backing store.
Process* read_first_process_from_backing_store() {
    FILE *fp = fopen(BACKING_STORE_FILENAME, "rb");
    if (!fp) return NULL;

    Process *p = malloc(sizeof(Process));
    if (!p) { fclose(fp); return NULL; }

    // 1. Read the main Process struct
    if (fread(p, sizeof(Process), 1, fp) != 1) {
        free(p);
        fclose(fp);
        return NULL; // File is empty or read error
    }

    // 2. Allocate memory and read instructions
    if (p->num_inst > 0) {
        p->instructions = malloc(sizeof(Instruction) * p->num_inst);
        if (!p->instructions) {
            free(p);
            fclose(fp);
            return NULL;
        }
        
        // Read the instructions
        if (fread(p->instructions, sizeof(Instruction), p->num_inst, fp) != p->num_inst) {
            free(p->instructions);
            free(p);
            fclose(fp);
            return NULL;
        }
        
        // Fix any potential dangling pointers in sub_instructions
        for (int i = 0; i < p->num_inst; i++) {
            p->instructions[i].sub_instructions = NULL;
        }
    } else {
        p->instructions = NULL;
    }

    // 3. Allocate memory and read variables
    if (p->num_var > 0) {
        p->variables = malloc(sizeof(Variable) * p->variables_capacity);
        if (!p->variables) {
            free(p->instructions);
            free(p);
            fclose(fp);
            return NULL;
        }
        
        // Read the variables
        if (fread(p->variables, sizeof(Variable), p->num_var, fp) != p->num_var) {
            free(p->instructions);
            free(p->variables);
            free(p);
            fclose(fp);
            return NULL;
        }
    } else {
        // FIX: A process must always have a valid, non-NULL variables array,
        // even if it's empty, to match the behavior of generate_dummy_process.
        int initial_capacity = p->variables_capacity > 0 ? p->variables_capacity : 8;
        p->variables = (Variable *)calloc(initial_capacity, sizeof(Variable));
        if (!p->variables) {
            if(p->instructions) free(p->instructions);
            free(p);
            fclose(fp);
            return NULL;
        }
        p->num_var = 0; // Ensure num_var is 0.
    }

    // 4. Make sure other pointers are initialized correctly
    p->logs = NULL;
    p->num_logs = 0;
    p->for_depth = 0;
    p->in_memory = 0;
    p->ticks_ran_in_quantum = 0;

    fclose(fp);
    return p;
}

// Removes the FIRST process from the backing store by rewriting the file without it.
void remove_first_process_from_backing_store() {
    FILE *fp = fopen(BACKING_STORE_FILENAME, "rb");
    if (!fp) {
        return; // File doesn't exist
    }

    // Read and skip the first process
    Process first_process;
    if (fread(&first_process, sizeof(Process), 1, fp) != 1) {
        fclose(fp);
        return; // Empty file or read error
    }

    // Validate and skip the first process's data
    if (first_process.num_inst < 0 || first_process.num_inst > 1000000 || 
        first_process.num_var < 0 || first_process.num_var > 1000000) {
        fclose(fp);
        return; // Corrupt data
    }

    long first_data_size = sizeof(Instruction) * first_process.num_inst + 
                          sizeof(Variable) * first_process.num_var;
    if (fseek(fp, first_data_size, SEEK_CUR) != 0) {
        fclose(fp);
        return; // Seek error
    }

    // Read the rest of the file into memory
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    long current_pos = sizeof(Process) + first_data_size;
    long remaining_size = file_size - current_pos;

    if (remaining_size <= 0) {
        // No more processes, just delete the file
        fclose(fp);
        remove(BACKING_STORE_FILENAME);
        return;
    }

    fseek(fp, current_pos, SEEK_SET);
    char *buffer = malloc(remaining_size);
    if (!buffer) {
        fclose(fp);
        return; // Memory allocation failed
    }

    size_t bytes_read = fread(buffer, 1, remaining_size, fp);
    fclose(fp);

    // Rewrite the file with remaining processes
    fp = fopen(BACKING_STORE_FILENAME, "wb");
    if (fp) {
        fwrite(buffer, 1, bytes_read, fp);
        fclose(fp);
    }

    free(buffer);
}

void print_backing_store_contents() {
    FILE *fp = fopen(BACKING_STORE_FILENAME, "rb");
    if (!fp) {
        printf("Backing store is empty or does not exist.\n");
        return;
    }

    printf("\n--- Backing Store Contents ---\n");
    int index = 0;
    Process p;

    // Read each process struct one by one
    while (fread(&p, sizeof(Process), 1, fp) == 1) {
        // Defensive: check for obviously invalid values
        if (p.num_inst < 0 || p.num_inst > 1000000 || p.num_var < 0 || p.num_var > 1000000) {
            printf("  [%d] Corrupt process entry detected (num_inst: %d, num_var: %d). Aborting print.\n", 
                   index, p.num_inst, p.num_var);
            break;
        }

        printf("[%d] PID: %d, Name: P%s, Instructions: %d, Variables: %d\n",
               index, p.pid, p.name, p.num_inst, p.num_var);

        // Calculate the size of the dynamic data that follows the struct
        long data_size = (sizeof(Instruction) * p.num_inst) + (sizeof(Variable) * p.num_var);

        // Seek past the dynamic data to get to the next process struct
        if (fseek(fp, data_size, SEEK_CUR) != 0) {
            printf("  Error seeking past data for PID %d. Backing store may be corrupt.\n", p.pid);
            break;
        }
        index++;
    }

    if (index == 0) {
        printf("Backing store is empty.\n");
    }
    printf("--- End of Backing Store ---\n\n");

    fclose(fp);
}