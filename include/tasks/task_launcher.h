
#ifndef __TASK_LAUNCHER_H__
#define __TASK_LAUNCHER_H__

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

void __attribute__ ((noreturn)) task_launcher(void);

#endif
