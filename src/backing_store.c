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
    printf("[DEBUG] Process %s (PID: %d) moved to backing store.\n", p->name, p->pid);
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
        if (!p->instructions || fread(p->instructions, sizeof(Instruction), p->num_inst, fp) != p->num_inst) {
            free(p->instructions);
            free(p);
            fclose(fp);
            return NULL;
        }
    } else {
        p->instructions = NULL;
    }

    // 3. Allocate memory and read variables
    if (p->num_var > 0) {
        p->variables = malloc(sizeof(Variable) * p->num_var);
        if (!p->variables || fread(p->variables, sizeof(Variable), p->num_var, fp) != p->num_var) {
            free(p->instructions);
            free(p->variables);
            free(p);
            fclose(fp);
            return NULL;
        }
    } else {
        p->variables = NULL;
    }

    fclose(fp);
    return p;
}

// Removes the FIRST process from the backing store by rewriting the file without it.
void remove_first_process_from_backing_store() {
    FILE *orig_fp = fopen(BACKING_STORE_FILENAME, "rb");
    if (!orig_fp) return;

    // Create a temporary file
    FILE *temp_fp = fopen("csopesy-backing-store.tmp", "wb");
    if (!temp_fp) {
        fclose(orig_fp);
        return;
    }

    // Seek past the first process in the original file
    Process temp_proc;
    if (fread(&temp_proc, sizeof(Process), 1, orig_fp) == 1) {
        long offset = sizeof(Instruction) * temp_proc.num_inst + sizeof(Variable) * temp_proc.num_var;
        fseek(orig_fp, offset, SEEK_CUR);

        // Copy the rest of the original file to the temp file
        char buffer[1024];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), orig_fp)) > 0) {
            fwrite(buffer, 1, bytes_read, temp_fp);
        }
    }

    fclose(orig_fp);
    fclose(temp_fp);

    // Replace the original file with the temporary one
    remove(BACKING_STORE_FILENAME);
    rename("csopesy-backing-store.tmp", BACKING_STORE_FILENAME);
    printf("[DEBUG] First process removed from backing store.\n");

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