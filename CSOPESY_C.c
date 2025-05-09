#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
// #include <threads.h> check documentations how to put this in cuz not work rn  

int main(){
    FILE *file = fopen("ascii.txt", "r");
    if(file == NULL){
        perror("Error opening file");
        return 1;
    }

    char line[256];
    while(fgets(line, sizeof(line), file)){
        printf("%s", line);
    }

    fclose(file);
    return 0;


}
