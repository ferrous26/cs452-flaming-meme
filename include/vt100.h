
#ifndef __VT100_H__
#define __VT100_H__

#include <std.h>
#include <io.h>
#include <syscall.h>

#define ESC "\x1B"
#define ESC_CODE ESC "["

void vt_init(void);
void vt_deinit(void);

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
#define printf(n, fmt, ...)						\
    {									\
	char print_buffer[n];						\
	char* str_end = sprintf(print_buffer, fmt, ## __VA_ARGS__);	\
	int result = Puts(print_buffer, str_end - print_buffer);	\
	assert(result == 0, "Failed to Puts (%d)", result);		\
	UNUSED(result);							\
    }

char* sprintf(char* buffer, const char* fmt, ...);


/**
 * Like sprintf(), except that it directly takes a va_list instead of
 * varargs. This is meant to be used by other APIs which wish to use
 * varargs and pass those varargs directly to sprintf().
 */
#define printf_va(n, fmt, args)						\
    {									\
	char print_buffer[n];						\
	char* str_end = sprintf_va(print_buffer, fmt, args);		\
	int result = Puts(print_buffer, str_end - print_buffer);	\
	assert(result == 0, "Failed to Puts (%d)", result);		\
	UNUSED(result);							\
    }

char* sprintf_va(char* buffer, const char* fmt, va_list args);


// general number printing, reasonably fast, but special case
// code will still be faster
char* sprintf_int(char* buffer, const int32 num);
char* sprintf_uint(char* buffer, const uint32 num);
char* sprintf_hex(char* buffer, const uint32 num);
char* sprintf_ptr(char* buffer, const void* const ptr);

char* sprintf_char(char* buffer, const char c);
char* sprintf_string(char* buffer, const char* str);

char* vt_clear_screen(char* buffer);
char* vt_goto_home(char* buffer);
char* vt_goto(char* buffer, const int row, const int column);
char* vt_hide_cursor(char* buffer);
char* vt_unhide_cursor(char* buffer);
char* vt_kill_line(char* buffer);
char* vt_reverse_kill_line(char* buffer);
char* vt_reset_scroll_region(char* buffer);
char* vt_set_scroll_region(char* buffer, int start, int end);
char* vt_save_cursor(char* buffer);
char* vt_restore_cursor(char* buffer);

// these do not seem to work...
char* vt_scroll_up(char* buffer);
char* vt_scroll_down(char* buffer);


// coloured output
typedef enum {
    BLACK   = 0,
    RED     = 1,
    GREEN   = 2,
    YELLOW  = 3,
    BLUE    = 4,
    MAGENTA = 5,
    CYAN    = 6,
    WHITE   = 7,
    DEFAULT = 7
} colour;

char* vt_colour_reset(char* buffer);
char* vt_colour(char* buffer, const colour c);

/**
 * The row where logging begins.
 */
#define LOG_HOME 18

/**
 * The row where logging ends.
 */
#define LOG_END  40

/**
 * To begin arbitrary logging (table, multiline things, etc.), use
 * vt_log_start() first, and then vt_log_end() to complete the
 * logging. Otherwise, use vt_log() with a format string for generic
 * logging as it is a wrapper around vt_log_start() and vt_log_end().
 */
char* log_start(char* buffer);
char* log_end(char* buffer);
void  log(const char* fmt, ...);
void  klog(const char* fmt, ...);

#endif
