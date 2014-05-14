/*
 * io.h - UART I/O methods
 */

#include <std.h>
#include <stdarg.h>

void uart_init();

#define DEBUG_HOME 5
#define DEBUG_HISTORY_SIZE 25
#define DEBUG_END  (DEBUG_HOME + DEBUG_HISTORY_SIZE)
void debug_message(const char* const message, ...);

// actually doing I/O
void vt_write();
void vt_read();
void vt_bwputc(const char c); // hold over from the old days

// buffered I/O
void vt_putc(const char c);
int  vt_getc();
int  vt_can_get();

// shortcuts
void vt_esc();
void vt_blank();
void vt_bwblank();
void vt_home();
void vt_hide();
void vt_kill_line();
void vt_goto(const uint8 row, const uint8 column);

// general number printing, reasonably fast, but special case
// code will be faster
void kprintf_int(const int32 num);
void kprintf_uint(const uint32 num);
void kprintf_hex(const uint32 num);
void kprintf_ptr(const void* const ptr);
void kprintf_string(const char* str);
void kprintf_bwstring(const char* str);

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

void vt_colour_reset();
void vt_colour(const colour c);

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
 *
 * Note that "\n" characters are automatically translated into "\r\n" in order
 * to handle serial I/O correctly.
 */
void kprintf(const char* const str, ...);
