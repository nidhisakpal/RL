#include "logger.h"
#include <stdlib.h>

csv_logger_t* csv_logger_open(const char* path) {
    csv_logger_t* lg = calloc(1, sizeof(csv_logger_t));
    lg->f = fopen(path, "w");
    fprintf(lg->f, "iteration,terminal_id,battery,covered\n");
    return lg;
}

void csv_logger_write(
    csv_logger_t* lg,
    int iteration,
    int terminal_id,
    double battery,
    int covered
) {
    if (!lg || !lg->f) return;

    fprintf(
        lg->f,
        "%d,%d,%.6f,%d\n",
        iteration,
        terminal_id,
        battery,
        covered
    );
}

void csv_logger_close(csv_logger_t* lg) {
    if (!lg) return;
    if (lg->f) fclose(lg->f);
    free(lg);
}
