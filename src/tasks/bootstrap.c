#include <io.h>
#include <debug.h>
#include <syscall.h>
#include <benchmark.h>
#include <hello.h>
#include <bootstrap.h>

static void print_created(int tid) {
    debug_log("Created: %d", tid);
    vt_flush();
}

BENCH(ctx);

void bootstrap(void) {
    print_created(Create(1, hello));
    print_created(Create(1, hello));
    print_created(Create(8, hello));
    print_created(Create(8, hello));

    BENCH_START(ctx);
    for (size i = 0; i < 10000; i++) {
	Pass();
	BENCH_LAP(ctx, 0);
    }

    BENCH_PRINT_WORST(ctx);
    BENCH_PRINT_AVERAGE(ctx);

    debug_log("First: exiting");
    vt_flush();
}
