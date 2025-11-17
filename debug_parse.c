#include <stdio.h>
#include <string.h>
int main() {
    char line[] = " % fs1: 10 5 4";
    char* trimmed = line;
    while (isspace(*trimmed)) trimmed++;
    printf("Original line: \"%s\"\n", line);
    printf("Trimmed line: \"%s\"\n", trimmed);
    printf("strstr(trimmed, \"%% fs\"): %s\n", strstr(trimmed, "% fs") ? "found" : "not found");
    printf("strstr(trimmed, \":\"): %s\n", strstr(trimmed, ":") ? "found" : "not found");
    
    int fst_id;
    if (sscanf(trimmed, "%% fs%d:", &fst_id) == 1) {
        printf("sscanf success: fst_id = %d\n", fst_id);
    } else {
        printf("sscanf failed\n");
    }
    return 0;
}
