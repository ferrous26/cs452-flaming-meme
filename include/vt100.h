
#ifndef __VT100_H__
#define __VT100_H__

#include <std.h>
#include <io.h>

void vt_init(void);
void vt_deinit(void);

void vt_clear_screen(void);
void vt_goto_home(void);
void vt_goto(const uint8 row, const uint8 column);
void vt_hide_cursor(void);
void vt_unhide_cursor(void);
void vt_kill_line(void);
void vt_reverse_kill_line(void);

// scroll up 1 line
void vt_scroll_up(void);

// scroll down 1 line
void vt_scroll_down(void);


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

void vt_colour_reset(void);
void vt_colour(const colour c);

#define LOG_HOME 8
#define LOG_END  40

/**
 * To begin arbitrary logging (table, multiline things, etc.), use
 * vt_log_start() first, and then vt_log_end() to complete the
 * logging. Otherwise, use vt_log() with a format string for generic
 * logging as it is a wrapper around vt_log_start() and vt_log_end().
 */
void vt_log_start(void);
void vt_log_end(void);
void vt_log(const char* fmt, ...);

#endif
