#include <kernel/arch/arm920/cache.h>

extern const int const _data_start;
extern const int const _data_hot_end;
extern const int const _data_warm_end;

extern const int const _text_hot_end;

#define FILL_SIZE 0x1F

void _flush_caches()
{
    // Invalidate the I/D-Cache
    asm volatile ("mov r0, #0                \n"
                  "mcr p15, 0, r0, c7, c7, 0 \n"
		  "mcr p15, 0, r0, c8, c7, 0 \n"
		  :
		  :
		  : "r0");
}

void _enable_caches()
{
    // Turn on the I-Cache
    asm volatile ("mrc p15, 0, r0, c1, c0, 0 \n" //get cache control
                  "orr r0, r0, #0x1000       \n" //turn I-Cache
		  "orr r0, r0, #5            \n" //turn on mmu and dcache #b0101
	          "mcr p15, 0, r0, c1, c0, 0 \n"
		  :
		  :
		  : "r0");
}

void _lockdown_icache()
{
    register uint r0 asm ("r0") = (uint)&_text_start & FILL_SIZE;
    register uint r1 asm ("r1") = (uint)&_text_hot_end;

    // Makes sure we lock up to the lowest point including the end defined here
    asm volatile ("mov      r2, #0                      \n"
                  "mcr      p15, 0, r2, c9, c0,  1      \n"
                  "ICACHE_LOOP:                         \n"
                  "mcr      p15, 0, r0, c7, c13, 1      \n"
                  "add      r0, r0, #32                 \n"
                  "and      r3, r0, #0xE0               \n"
                  "cmp      r3, #0                      \n"
                  "addeq    r2, r2, #1 << 26            \n"
                  "mcreq    p15, 0, r2, c9, c0,  1      \n"
                  "cmp      r0, r1                      \n"
                  "ble      ICACHE_LOOP                 \n"
                  "cmp      r3, #0                      \n"
                  "addne    r2, r2, #1 << 26            \n"
                  "mcrne    p15, 0, r2, c9, c0,  1      \n"
                  :
                  : "r" (r0), "r" (r1)
                  : "r2", "r3" );
}

void _lockdown_dcache()
{
    const uint warm_chunk =
        (((uint)&_data_warm_end - (uint)&_data_hot_end) / sizeof(task)) *
        TASKS_TO_LOCKDOWN;

    register uint r0 asm ("r0") = (uint)&_data_start & FILL_SIZE;
    register uint r1 asm ("r1") = (uint)&_data_hot_end + (uint)warm_chunk;

    // Makes sure we lock up to the lowest point including the end defined here
    asm volatile ("mov      r2, #0                      \n"
                  "mcr      p15, 0, r2, c9, c0,  0      \n"
                  "DCACHE_LOOP:                         \n"
                  "ldr      r3, [r0], #32               \n"
                  "and      r3, r0, #0xE0               \n"
                  "cmp      r3, #0                      \n"
                  "addeq    r2, r2, #1 << 26            \n"
                  "mcreq    p15, 0, r2, c9, c0,  0      \n"
                  "cmp      r0, r1                      \n"
                  "ble      DCACHE_LOOP                 \n"
                  "cmp      r3, #0                      \n"
                  "addne    r2, r2, #1 << 26            \n"
                  "mcrne    p15, 0, r2, c9, c0,  0      \n"
                  :
                  : "r" (r0), "r" (r1)
                  : "r2", "r3" );
}
