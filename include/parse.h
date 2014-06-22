#ifndef __PARSE_H__
#define __PARSE_H__

typedef enum {
    NONE,

    CMD_STRESS,
    CMD_BENCHMARK,

    TRACK_RESET,
    TRACK_TURNOUT,
    
    LOC_LIGHT,
    LOC_SPEED,
    LOC_REVERSE,
    
    QUIT,
    ERROR,
} command;

command parse_command(const char* const cmd, int* const buffer);

#endif
