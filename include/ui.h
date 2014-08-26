
#ifndef __UI_H__
#define __UI_H__

#include <std.h>

#define TRAIN_BANDWIDTH_ROW   1
#define TRAIN_BANDWIDTH_COL   20
#define TRAIN_BANDWIDTH_UP    27
#define TRAIN_BANDWIDTH_WIDTH 3

/**
 * Append white space to a format string (sprintf style)
 *
 * Given the amount of space that should be used, and the amount of space
 * that has already been used, fill in the rest of the space with...space.
 *
 * @return Updated format string pointer
 */
char* ui_pad(char* ptr, const size_t used_width, const size_t total_width);

/**
 * Busy indicator
 *
 * Feed it the current symbol and get the next symbol to print out.
 */
char __attribute__ ((const)) ui_twirl(const char prev);

#endif
