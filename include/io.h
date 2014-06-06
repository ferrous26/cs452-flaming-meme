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
bool vt_can_get(void);

// general number printing, reasonably fast, but special case
// code will still be faster
void kprintf_int(const int32 num);
void kprintf_uint(const uint32 num);
void kprintf_hex(const uint32 num);
void kprintf_ptr(const void* const ptr);
void kprintf_string(const char* str);
void kprintf_char(const char c);

/**
 * Similar to printf(3), except it only supports a subset of format specifiers.
 *
 * %d - signed decimal
 * %u - unsigned decimal
 * %o - unsigned octal
 * %x - unsigned hexidecimal
 * %f - floating point (CURRENTLY NOT IMPLEMENTED)
 * %p - pointer
 * %s - string (safely non-recursive)
 * %c - char
 * %% - literal `%'
 */
void kprintf(const char* const fmt, ...);
void kprintf_va(const char* const fmt, va_list args);

#endif
