#include <stdio.h>
int main() {
    char line[] = "% fs1: 10 5 4";
    int fst_id;
    if (sscanf(line, "%% fs%d:", &fst_id) == 1) {
        printf("Found fst_id: %d\n", fst_id);
    } else {
        printf("No match\n");
    }
    return 0;
}
