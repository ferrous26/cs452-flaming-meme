#include <std.h>

// shortcuts for
void vt_esc();
void vt_blank();
void vt_home();
void vt_hide();
void vt_kill_line();
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

void vt_colour_reset();
void vt_colour(const colour c);
