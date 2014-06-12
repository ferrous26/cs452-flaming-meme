/*
 * io.h - UART I/O methods
 */

#ifndef __IO_H__
#define __IO_H__

#include <std.h>
#include <stdarg.h>

void uart_init(void);

#define kprintf(n, fmt, ...)					\
    {								\
	char sbuffer[n];					\
	char* end = sprintf(sbuffer, fmt, ## __VA_ARGS__);	\
	uart2_bw_write(sbuffer, (uint)(end - sbuffer));		\
    }

#define kprintf_va(n, fmt, args)			\
    {							\
	char sbuffer[n];				\
	char* end = sprintf_va(sbuffer, fmt, args);	\
	uart2_bw_write(sbuffer, (uint)(end - sbuffer));	\
    }


// actually doing I/O
void uart2_bw_write(const char* string, uint length);

bool __attribute__ ((pure)) uart2_bw_can_read(void);
char uart2_bw_read(void);
char uart2_bw_waitget(void);

void irq_uart2(void);

#endif
