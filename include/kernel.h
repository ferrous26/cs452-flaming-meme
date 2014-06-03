
#ifndef __KERNEL_H__
#define __KERNEL_H_

#include <std.h>

void exit_to_redboot(void* ep) __attribute__ ((noreturn));
void shutdown(void) __attribute__((noreturn));

#endif
