#include <debug.h>
#include <std.h>
#include <syscall.h>
#include <scheduler.h>
#include <benchmark.h>

BENCH(benchbench);

void papers() {

    if (myPriority() != TASK_PRIORITY_MAX) return;

    BENCH_START(benchbench);
    for (size i = 1000000; i; i--) {
	BENCH_LAP(benchbench);
    }

    BENCH_PRINT_WORST(benchbench);
    BENCH_PRINT_AVERAGE(benchbench);
    vt_flush();

}
