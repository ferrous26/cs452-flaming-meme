#ifndef __PARSE_H__
#define __PARSE_H__

typedef enum {
    NONE,

    CMD_STRESS,
    CMD_BENCHMARK,
    CMD_ECHO,
    REVERSE_LOOKUP,

    TRACK_RESET,
    TRACK_TURNOUT,
    SWITCH_STOP,
    SWITCH_TIME,

    LOC_LIGHT,
    LOC_SPEED,
    LOC_REVERSE,
    LOC_HORN,

    QUIT,
    ERROR,
} command;

command parse_command(const char* const cmd, int* const buffer);

#endif
