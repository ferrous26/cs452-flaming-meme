
#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <std.h>
#include <syscall.h>

#define TEXT_HOT __attribute__ ((section (".text.kern")))
#define DATA_HOT __attribute__ ((section (".data.kern")))

void __attribute__ ((noreturn)) shutdown(void);
void __attribute__ ((noreturn)) abort(const kreq_abort* const req);


#endif
