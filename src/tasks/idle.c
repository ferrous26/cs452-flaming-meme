#include <tasks/idle.h>
#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <std.h>
#include <clock.h>
#include <debug.h>
#include <syscall.h>

// best case number of Timer4 ticks for a context switch
#define MINIMUM_NON_IDLE_TIME 2

#define IDLE_ASSERT if (result) idle_failure(result, __LINE__)
static void __attribute__ ((noreturn)) idle_failure(int result, uint line) {
    log("Action failed in idle UI at line %u (%d)", line, result);
    Shutdown();
}

static uint non_idle_ticks = 0;

static inline char twirler(const char prev) {
    switch (prev) {
    case  '/': return '-';
    case  '-': return '\\';
    case '\\': return '|';
    case  '|': return '/';
    }
    return '|';
}

static void __attribute__ ((noreturn)) idle_ui() {

// IDLE nn%
#define IDLE_ROW 2
#define IDLE_COL 7

    char twirl = '/';
    char buffer[32];
    char*  ptr = buffer;

    int result = RegisterAs((char*)IDLE_UI_NAME);
    if (result != 0) ABORT("Idle UI Has Failed to Load (%d)", result);

    ptr = vt_goto(buffer, IDLE_ROW, 1);
    ptr = sprintf_string(ptr, "IDLE  00");
    Puts(buffer, ptr - buffer);

    int time = 0;

    FOREVER {
        time = Delay(100);

        // 9830.4 idle ticks = 1 clock tick
        // we update the UI once every 1000 milliseconds
        // so, 983040 idle ticks = 100 clock ticks = 1 UI update
        // therefore, total number of idle ticks is given by this
#define T4_TICKS_PER_SECOND 983040
        uint idle_time = (T4_TICKS_PER_SECOND - non_idle_ticks) * 100;
        idle_time /= T4_TICKS_PER_SECOND;
        non_idle_ticks = 0;

        if (idle_time >= 100) idle_time = 99; // handle this edge case

        ptr = vt_goto(buffer, IDLE_ROW, IDLE_COL);
        ptr = sprintf(ptr, "%c%c%c",
		      '0' + (idle_time / 10),
                      '0' + (idle_time % 10),
		      (twirl = twirler(twirl)));
        Puts(buffer, ptr - buffer);
    }
}

void idle() {
    non_idle_ticks = 0;

    int result = RegisterAs((char*)IDLE_NAME);
    if (result != 0) ABORT("Idle failed to register (%d)", result);

    result = Create(TASK_PRIORITY_MEDIUM_LOW, idle_ui);
    if (result < 0) ABORT("Idle UI task failed to start (%d)", result);

    uint prev_time = clock_t4tick();
    uint curr_time = prev_time;
    uint diff      = 0;

    FOREVER {
        curr_time = clock_t4tick();
        diff      = curr_time - prev_time;
        prev_time = curr_time;

        if (diff > MINIMUM_NON_IDLE_TIME)
            non_idle_ticks += diff;
    }
}
