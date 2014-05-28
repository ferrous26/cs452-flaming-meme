#include <tasks/memcpy_bench.h>
#include <benchmark.h>
#include <syscall.h>
#include <vt100.h>
#include <memory.h>

BENCH(mem4);
BENCH(mem8);
BENCH(mem16);
BENCH(mem32);
BENCH(mem64);
BENCH(mem128);
BENCH(mem28);
BENCH(mem3);
//BENCH(mema);

static char copy[256];
static const char* str =
    "this is a very l"
    "ong string that "
    "is at least 256 "
    "characters long."
    "..I think, but I"
    " can always make"
    " it longer by dr"
    "oning on and on "
    "and on about som"
    "ething or rather"
    "...maybe, or may"
    "be not. this is "
    "harder than I th"
    "ought it was. I "
    "probably should "
    "have just used t"
    "he lorem ipsum g"
    "enerator to get "
    "a giant block of"
    " text to futz wi"
    "th for this benc"
    "hmark.........:)";

void reset() {
    // TODO: implement memset
    for (size i = 0; i < 256; i++) {
	copy[i] = '\0';
    }
}

void memcpy_bench(void) {
    vt_log("MEMCPY BENCH STARTING...");
    vt_flush();

    uint count = 0;

    vt_log("copy  (%p)", copy);
    vt_log("str   (%p)", str);

#define MEMTEST(name, siz)			\
    reset();					\
    BENCH_START(name);				\
    for (count = 10000; count; count--) {	\
	memcpy(copy, str, siz);			\
	BENCH_LAP(name);			\
    }						\
    BENCH_PRINT_WORST(name);			\
    BENCH_PRINT_AVERAGE(name);			\
    vt_log("copy = %s", copy);			\
    vt_log("");					\
    vt_flush();

    MEMTEST(mem4, 4);
    MEMTEST(mem8,8);
    MEMTEST(mem16, 16);
    MEMTEST(mem32, 32);
    MEMTEST(mem64, 64);
    MEMTEST(mem128, 128);
    MEMTEST(mem3, 3);
    MEMTEST(mem28, 28);

    /* reset(); */
    /* BENCH_START(mema); */
    /* for (count = 10000; count; count--) { */
    /* 	memcpy(copy+3, str, 32); */
    /* 	BENCH_LAP(mema); */
    /* } */
    /* BENCH_PRINT_WORST(mema); */
    /* BENCH_PRINT_AVERAGE(mema); */
    /* vt_log("copy = %s", copy); */
    /* vt_log(""); */
    /* vt_flush(); */

    vt_log("MEMCPY BENCH DONE!");
    vt_flush();
}
