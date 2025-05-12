#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
// #include <threads.h> check documentations how to put this in cuz not work rn  

// Colors use for design
#define yellow "\x1b[33m"
#define green "\x1b[32m"
#define reset "\x1b[0m"

// Prototype so as to only see main at top
void header();
void initialize();
void screen();
void scheduler_test();
void scheduler_stop();
void report_util();
void clear();

// design decision
void print_color(const char *color, const char *text);

int main(){
    char str[100];
    header();
    printf("");
    print_color(green, "Hello! Welcome to PEFE-OS command line!\n");
    print_color(yellow, "Type 'exit' to quite, 'clear' to clear the screen\n");

    printf("Enter command: \n");
    fgets(str, sizeof(str), stdin);

    str[strcspn(str, "\n")] = '\0';

    if(strcmp(str, "exit") == 0){
        printf("Goodbye!");
        return 0;
    }

    if(strcmp(str, "clear") == 0){
        clear();
    }
    return 0;
}

void header(){

    FILE *file = fopen("ascii.txt", "r");
    if(file == NULL){
        perror("Error opening file");
    }

    char line[256];
    while(fgets(line, sizeof(line), file)){
        printf("%s", line);
    }
    fclose(file);

}

void print_color(const char *color, const char *text){
    printf("%s%s%s", color, text, reset);
}

void initialize(){
    printf(" command recognized. Doing something");
}

void screen(){
    printf(" command recognized. Doing something");
}

void scheduler_test(){
    printf(" command recognized. Doing something");
}

void scheduler_stop(){
    printf(" command recognized. Doing something");
}

void report_util(){
    printf(" command recognized. Doing something");
}

void clear(){

    printf("\x1b[2J\x1b[H");
    main();
}
