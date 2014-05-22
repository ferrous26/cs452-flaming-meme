#include <tasks/memcpy_bench.h>
#include <benchmark.h>
#include <syscall.h>
#include <vt100.h>
#include <memory.h>

BENCH(mem8);
BENCH(mem16);
BENCH(mem32);
BENCH(mem64);
BENCH(mem3);
BENCH(mema);

static char copy[256];
static const char* str =
    "this is a very long string that is "
    "at least 256 characters long...I think, "
    "but I can always make it longer by droning on and on and on "
    "about something or rather...maybe, or maybe not."
    "this is harder than I thought it was. I probably should have "
    "just used the lorem ipsum generator to get a giant block of "
    "text to futz with for this benchmark";

void memcpy_bench(void) {
    vt_log("MEMCPY BENCH STARTING...");
    vt_flush();

   uint count = 0;

   BENCH_START(mem8);
   for (count = 10000; count; count--) {
       memcpy(copy, str, 8);
       BENCH_LAP(mem8, 0);
   }

   BENCH_START(mem16);
   for (count = 10000; count; count--) {
       memcpy(copy, str, 16);
       BENCH_LAP(mem16, 0);
   }

   BENCH_START(mem32);
   for (count = 10000; count; count--) {
       memcpy(copy, str, 32);
       BENCH_LAP(mem32, 0);
   }

   BENCH_START(mem64);
   for (count = 10000; count; count--) {
       memcpy(copy, str, 64);
       BENCH_LAP(mem64, 0);
   }

   BENCH_START(mem3);
   for (count = 10000; count; count--) {
       memcpy(copy, str, 3);
       BENCH_LAP(mem3, 0);
   }

   BENCH_START(mema);
   for (count = 10000; count; count--) {
       memcpy(copy+3, str, 32);
       BENCH_LAP(mema, 0);
   }

   BENCH_PRINT_WORST(mem8);
   BENCH_PRINT_WORST(mem16);
   BENCH_PRINT_WORST(mem32);
   BENCH_PRINT_WORST(mem64);
   BENCH_PRINT_WORST(mem3);
   BENCH_PRINT_WORST(mema);

   BENCH_PRINT_AVERAGE(mem8);
   BENCH_PRINT_AVERAGE(mem16);
   BENCH_PRINT_AVERAGE(mem32);
   BENCH_PRINT_AVERAGE(mem64);
   BENCH_PRINT_AVERAGE(mem3);
   BENCH_PRINT_AVERAGE(mema);

   vt_log("copy (%p) = %s", copy, copy);
   vt_log("str (%p)  = %s", str, str);

   vt_log("MEMCPY BENCH DONE!");
   vt_flush();
}
