/*
 * io.h - UART I/O methods
 */

#ifndef __IO_H__
#define __IO_H__

#include <std.h>
#include <kernel.h>

void uart_init(void) TEXT_COLD;

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
void uart2_bw_write(const char* string, int length) TEXT_COLD;

bool __attribute__ ((pure)) uart2_bw_can_read(void) TEXT_COLD;
char uart2_bw_read(void) TEXT_COLD;
char uart2_bw_waitget(void) TEXT_COLD;


void irq_uart2(void);
void irq_uart2_send(void) TEXT_HOT;
void irq_uart2_recv(void);

void irq_uart1(void) TEXT_HOT;
void irq_uart1_send(void) TEXT_HOT;
void irq_uart1_recv(void) TEXT_HOT;

char* klog_start(char* buffer);
void  klog(const char* fmt, ...);
#define klog_end log_end

#endif
