#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef struct {
    FILE* f;
} csv_logger_t;

csv_logger_t* csv_logger_open(const char* path);
void csv_logger_write(
    csv_logger_t* lg,
    int iteration,
    int terminal_id,
    double battery,
    int covered
);
void csv_logger_close(csv_logger_t* lg);

#endif
