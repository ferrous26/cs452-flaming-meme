#include <tasks/idle.h>
#include <std.h>
#include <clock.h>
#include <debug.h>
#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>

// best case number of Timer4 ticks for a context switch
#define MINIMUM_NON_IDLE_TIME 2

#define IDLE_ASSERT if (result) idle_failure(result, __LINE__)
static void __attribute__ ((noreturn)) idle_failure(int result, uint line) {
    log("Action failed in idle UI at line %u (%d)", line, result);
    Shutdown();
}

static uint non_idle_ticks = 0;


static void __attribute__((noreturn)) idle_ui() {

// IDLE nn%
#define IDLE_ROW 2
#define IDLE_COL 7

    log("Idle UI starting on %d", myTid());

    char buffer[32];
    char*   ptr = buffer;
    int  result = 0;

    ptr = vt_goto(buffer, IDLE_ROW, 1);
    ptr = sprintf_string(ptr, "IDLE  00%");
    result = Puts(buffer, (int)(ptr - buffer));
    IDLE_ASSERT;

    int time = 0;

    FOREVER {
        Delay(100);
        time = Time();

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
        ptr = sprintf(ptr, "%c%c",
		      '0' + (idle_time / 10),
                      '0' + (idle_time % 10));
        result = Puts(buffer, (int)(ptr - buffer));
        IDLE_ASSERT;
    }
}

void idle() {

    log("Starting idle task at %d", myTid());

    non_idle_ticks = 0;
    int result = Create(TASK_PRIORITY_MEDIUM_LO, idle_ui);
    UNUSED(result);
    assert(result > 0, "Idle UI task failed to start (%d)", result);

    // now that we are done setting up, we can drop to idle priority
    ChangePriority(TASK_PRIORITY_IDLE);

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
