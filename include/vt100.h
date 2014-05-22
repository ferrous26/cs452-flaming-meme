
#ifndef __VT100_H__
#define __VT100_H__

#include <std.h>
#include <io.h>

void vt_init(void);

void vt_blank(void);
void vt_home(void);
void vt_hide(void);
void vt_kill_line(void);
void vt_goto(const uint8 row, const uint8 column);

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

void vt_log_start(void);
void vt_log_end(void);
void vt_log(const char* fmt, ...);

#endif
