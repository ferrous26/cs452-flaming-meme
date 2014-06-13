#include <tasks/bench_memcpy.h>
#include <vt100.h>
#include <benchmark.h>

BENCH(mem4);
BENCH(mem8);
BENCH(mem16);
BENCH(mem32);
BENCH(mem64);
BENCH(mem28);
BENCH(mem3);
BENCH(mem1);
BENCH(mema);
BENCH(mema1);
BENCH(memse);

static char copy[256];
static const char* str = NULL;

void bench_memcpy(void) {
    log("MEMCPY BENCH STARTING...");

    uint count = 0;

    str =
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

    log("copy  (%p)", copy);
    log("str   (%p)", str);

#define MEMTEST(name, siz)			\
    memset(copy, 0, 256);			\
    BENCH_START(name);				\
    for (count = 10000; count; count--) {	\
	memcpy(copy, str, siz);			\
	BENCH_LAP(name);			\
    }						\
    BENCH_PRINT_WORST(name);			\
    BENCH_PRINT_AVERAGE(name);			\
    log("copy = %s\n", copy);			\

    MEMTEST(mem4,  4);
    MEMTEST(mem8,  8);
    MEMTEST(mem16, 16);
    MEMTEST(mem32, 32);
    MEMTEST(mem64, 64);
    MEMTEST(mem3,  3);
    MEMTEST(mem1,  1);
    MEMTEST(mem28, 28);

    memset(copy, 0, 256);
    BENCH_START(mema);
    for (count = 10000; count; count--) {
    	memcpy(copy, str+3, 32);
    	BENCH_LAP(mema);
    }
    BENCH_PRINT_WORST(mema);
    BENCH_PRINT_AVERAGE(mema);
    log("copy = %s", copy);
    log("");

    memset(copy, 0, 256);
    BENCH_START(mema1);
    for (count = 10000; count; count--) {
    	memcpy(copy+1, str+1, 1);
    	BENCH_LAP(mema1);
    }
    BENCH_PRINT_WORST(mema1);
    BENCH_PRINT_AVERAGE(mema1);
    log("copy = %s", copy+1);
    log("");


    log("MEMCPY BENCH DONE!");


    log("MEMSET BENCH STARTING...");
    memset(copy, 0, 256);
    BENCH_START(memse);
    for (count = 10000; count; count--) {
	memset(copy, 97, 16);
	BENCH_LAP(memse);
    }
    BENCH_PRINT_WORST(memse);
    BENCH_PRINT_AVERAGE(memse);
    log("copy = %s", copy);
    log("MEMSET BENCH DONE!");
}
