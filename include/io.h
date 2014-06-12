/*
 * io.h - UART I/O methods
 */

#ifndef __IO_H__
#define __IO_H__

#include <std.h>
#include <stdarg.h>

void uart_init(void);

// actually doing I/O
void vt_write(void);
void vt_read(void);
void vt_flush(void); // flush the entire output buffer

// buffered I/O
char vt_getc(void);
bool __attribute__ ((pure)) vt_can_get(void);
char vt_waitget(void);

void kprintf_string(const char* str, uint strlen);

void irq_uart2(void);

#endif
