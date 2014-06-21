#ifndef __PARSE_H__
#define __PARSE_H__

typedef enum {
    NONE = 0,
    SPEED,
    GATE,
    REVERSE,
    QUIT,
    TOGGLE_LIGHT,
    RESET,
    ERROR
} command;

command parse_command(const char* const cmd, int* const buffer);

#endif
