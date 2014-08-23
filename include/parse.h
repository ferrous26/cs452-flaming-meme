#ifndef __PARSE_H__
#define __PARSE_H__

typedef enum {
    NONE,

    CMD_STRESS,
    CMD_BENCHMARK,
    CMD_ECHO,

    TRACK_RESET,
    TRACK_TURNOUT,

    LOC_SPEED,
    LOC_REVERSE,
    LOC_HORN,

    SIZES,
    SEPPUKU,

    TEST_TIME,

    QUIT,
    ERROR,
} command;

command parse_command(const char* const cmd, int* const buffer);

#endif
