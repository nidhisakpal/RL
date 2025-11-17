#ifndef DEBUG_CONTROL_H
#define DEBUG_CONTROL_H

/* Debug output control macros */
/* Comment out the line below to disable verbose debug output */
// #define ENABLE_DEBUG_VERBOSE

#ifdef ENABLE_DEBUG_VERBOSE
    #define DEBUG_PRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) do {} while(0)
#endif

#endif /* DEBUG_CONTROL_H */