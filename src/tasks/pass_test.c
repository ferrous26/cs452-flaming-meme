#include <io.h>
#include <debug.h>
#include <syscall.h>
#include <benchmark.h>

BENCH(ctx);

void pass_test(void) {
    debug_log("PASS TEST STARTING...");
    vt_flush();
    
    BENCH_START(ctx);
    for (size i = 0; i < 1000000; i++) {
	Pass();
	BENCH_LAP(ctx, 0);
    }

    BENCH_PRINT_WORST(ctx);
    BENCH_PRINT_AVERAGE(ctx);

    debug_log("PASS TEST DONE!");
    vt_flush();
}
