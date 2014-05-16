#include <std.h>

void vt_init(void);

void vt_esc(void);
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
