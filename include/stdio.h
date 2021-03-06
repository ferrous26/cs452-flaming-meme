#ifndef __STDIO_H__
#define __STDIO_H__

#include <std.h>
#include <stdarg.h>

/**
 * Similar to sprintf(3), except it only supports a subset of format specifiers.
 *
 * %d - signed decimal
 * %u - unsigned decimal
 * %o - unsigned octal
 * %x - unsigned hexidecimal
 * %p - pointer
 * %s - string (non-recursive)
 * %c - char
 * %% - literal `%'
 */
#define printf(fmt, ...)						\
    {									\
	char print_buffer[256];						\
	char* str_end = sprintf(print_buffer, fmt, ## __VA_ARGS__);	\
	Puts(print_buffer, str_end - print_buffer);			\
    }

char* sprintf(char* buffer, const char* fmt, ...);


/**
 * Like sprintf(), except that it directly takes a va_list instead of
 * varargs. This is meant to be used by other APIs which wish to use
 * varargs and pass those varargs directly to sprintf().
 */
#define printf_va(fmt, args)						\
    {									\
	char print_buffer[256];						\
	char* str_end = sprintf_va(print_buffer, fmt, args);		\
	Puts(print_buffer, str_end - print_buffer);			\
    }

char* sprintf_va(char* buffer, const char* fmt, va_list args);


// general number printing, reasonably fast, but special case
// code will still be faster
char* sprintf_int(char* buffer, const int32 num);
char* sprintf_uint(char* buffer, const uint32 num);
char* sprintf_hex(char* buffer, const uint32 num);
char* sprintf_ptr(char* buffer, const void* const ptr);

static inline char* sprintf_char(char* buffer, const char c) {
    *(buffer++) = c;
    return buffer;
}

char* sprintf_string(char* buffer, const char* str);

#endif
