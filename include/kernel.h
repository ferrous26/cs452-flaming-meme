
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <std.h>
#define TEXT_HOT __attribute__ ((section (".text.kern")))
#define DATA_HOT __attribute__ ((section (".data.kern")))

void shutdown(void);

#endif
