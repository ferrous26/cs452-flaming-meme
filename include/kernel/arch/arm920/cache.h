#include <kernel.h>

// flush caches before enabling them
void _flush_caches() TEXT_COLD;
void _enable_caches() TEXT_COLD;

// ARM920 supports explicit loading of cache chunks and marking those
// chunks as being non-evictable
void _lockdown_icache() TEXT_COLD;
void _lockdown_dcache() TEXT_COLD;
