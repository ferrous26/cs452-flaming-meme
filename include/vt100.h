
#ifndef __VT100_H__
#define __VT100_H__

#include <std.h>
#include <io.h>
#include <syscall.h>

#define ESC "\x1B"
#define ESC_CODE ESC "["

void vt_init(void) TEXT_COLD;
void vt_deinit(void) TEXT_COLD;

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
	Puts(print_buffer, str_end - print_buffer);			\
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
#define BLACK          "30"
#define RED            "31"
#define GREEN          "32"
#define YELLOW         "33"
#define BLUE           "34"
#define MAGENTA        "35"
#define CYAN           "36"
#define LIGHT_GRAY     "37"
#define DARK_GRAY      "90"
#define LIGHT_RED      "91"
#define LIGHT_GREEN    "92"
#define LIGHT_YELLOW   "93"
#define LIGHT_BLUE     "94"
#define LIGHT_MAGENTA  "95"
#define LIGHT_CYAN     "96"
#define WHITE          "97"

#define DEFAULT        "39"

#define BG_BLACK          "40"
#define BG_RED            "41"
#define BG_GREEN          "42"
#define BG_YELLOW         "43"
#define BG_BLUE           "44"
#define BG_MAGENTA        "45"
#define BG_CYAN           "46"
#define BG_LIGHT_GRAY     "47"
#define BG_DARK_GRAY      "100"
#define BG_LIGHT_RED      "101"
#define BG_LIGHT_GREEN    "102"
#define BG_LIGHT_YELLOW   "103"
#define BG_LIGHT_BLUE     "104"
#define BG_LIGHT_MAGENTA  "105"
#define BG_LIGHT_CYAN     "106"
#define BG_WHITE          "107"

#define COLOUR_RESET ESC_CODE "0m"
#define COLOUR_SUFFIX "m"
#define COLOUR(c) ESC_CODE c COLOUR_SUFFIX

#define vt_colour_reset(ptr) sprintf_string(ptr, COLOUR_RESET)


/**
 * The row where logging begins.
 */
#define LOG_HOME 18

/**
 * The row where logging ends.
 */
#define LOG_END  99

/**
 * To begin arbitrary logging (table, multiline things, etc.), use
 * vt_log_start() first, and then vt_log_end() to complete the
 * logging. Otherwise, use vt_log() with a format string for generic
 * logging as it is a wrapper around vt_log_start() and vt_log_end().
 */
char* log_start(char* buffer);
char* clog_start(char* buffer);
char* klog_start(char* buffer);
char* log_end(char* buffer);
#define klog_end log_end
void  log(const char* fmt, ...);
void  clog(const char* fmt, ...); // non-timestamped log
void  klog(const char* fmt, ...);

#ifdef NIK
#define nik_log( fmt, ... ) log(fmt, ##__VA_ARGS__ )
#else
#define nik_log( ... )
#endif

#ifdef MARK
#define mark_log(fmt, ... ) log(fmt, ##__VA_ARG__ )
#else
#define mark_log( ... )
#endif

#endif
