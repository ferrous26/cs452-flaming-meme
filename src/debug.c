#include <clock.h>
#include <io.h>
#include <vt100.h>

static unsigned int worst_loop = 0;
static unsigned int current_loop = 0;
static unsigned int previous_loop = 0;

static unsigned int avg_pollrespond = 0;
static unsigned int avg_pollrespondall = 0;

static unsigned int profile1 = 0;
static unsigned int poll_start = 0;

void debug_init() {

    worst_loop = 0;
    clock_t4enable();
    previous_loop = clock_t4tick();

    poll_start = 0;
    profile1 = 0;
    avg_pollrespond = 0;
    avg_pollrespondall = 0;
}

void debug_loop() {
    current_loop = clock_t4tick();
    unsigned int nTicks = current_loop - previous_loop;

    if( nTicks > worst_loop ) {
        worst_loop = nTicks;
    }

    previous_loop = current_loop;
}

void debug_pollsent() {
    profile1 = 1;
    poll_start = clock_t4tick();
}

void debug_responce() {
    if ( profile1 == 0 ) return;
    int nTicks = clock_t4tick() - poll_start;

    if( avg_pollrespond == 0 ) {
        avg_pollrespond = nTicks;
    } else {
        avg_pollrespond = (avg_pollrespond + nTicks) / 2;
    }

    profile1 = 0;
}

void debug_allresponces() {
    int nTicks = clock_t4tick() - poll_start;

    if( avg_pollrespond == 0 ) {
        avg_pollrespondall = nTicks;
    } else {
        avg_pollrespondall = (avg_pollrespondall + nTicks) / 2;
    }
}

void debug_print( ) {
    vt_goto(6, 70);
    kprintf( "%d", worst_loop );

    vt_goto(8, 70);
    kprintf("%d", avg_pollrespond);

    vt_goto(9, 70);
    kprintf("%d", avg_pollrespondall);
}
