#include <tasks/term_server.h>
#include <tasks/clock_server.h>
#include <vt100.h>

char* vt_clear_screen(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "2J");
}

char* vt_goto_home(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "H");
}

static char* vt_set_location(char* buffer, const int first, const int second) {
    assert(first >= 0 && first < 100,
           "vt_set_location first is invalid (%d)", first);
    assert(second >= 0 &&second < 100,
           "vt_set_loaction second is invalid (%d)", second);

    const char first_high  = (char)(first / 10)  + '0';
    const char first_low   = (char)(first % 10)  + '0';
    const char second_high = (char)(second / 10) + '0';
    const char second_low  = (char)(second % 10) + '0';

    buffer = sprintf_string(buffer, ESC_CODE);
    if (first_high > 48)
	buffer = sprintf_char(buffer, first_high);
    buffer = sprintf_char(buffer, first_low);
    buffer = sprintf_char(buffer, ';');
    if (second_high > 48)
	buffer = sprintf_char(buffer, second_high);
    return sprintf_char(buffer, second_low);
}

char* vt_goto(char* buffer, const int row, const int column) {
    buffer = vt_set_location(buffer, row, column);
    return sprintf_char(buffer, 'H');
}

char* vt_hide_cursor(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "?25l");
}

char* vt_unhide_cursor(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "?25h");
}

char* vt_kill_line(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "K");
}

char* vt_reverse_kill_line(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "1K");
}

char* vt_set_scroll_region(char* buffer, int start, int end) {
    buffer = vt_set_location(buffer, start, end);
    return sprintf_char(buffer, 'r');
}

char* vt_reset_scroll_region(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "r");
}

char* vt_scroll_up(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "1S");
}

char* vt_scroll_down(char* buffer) {
    return sprintf_string(buffer, ESC_CODE "1T");
}

char* vt_save_cursor(char* buffer) {
    return sprintf_string(buffer, ESC "7");
}

char* vt_restore_cursor(char* buffer) {
    return sprintf_string(buffer, ESC "8");
}

char* log_start(char* buffer) {
    buffer = vt_restore_cursor(buffer);
    buffer = sprintf_uint(buffer, (uint)Time());
    return sprintf_string(buffer, ": ");
}

char* clog_start(const int time, char* buffer) {
    buffer = vt_restore_cursor(buffer);
    return sprintf(buffer, "%d: ", time);
}

char* log_end(char* buffer) {
    buffer = sprintf_string(buffer, "\n");
    return vt_save_cursor(buffer);
}

void log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[256];
    char* ptr = buffer;

    ptr = log_start(ptr);
    ptr = sprintf_va(ptr, fmt, args);
    ptr = log_end(ptr);

    Puts(buffer, ptr - buffer);
    va_end(args);
}

void clog(const int time, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[256];
    char* ptr = buffer;

    ptr = clog_start(time, ptr);
    ptr = sprintf_va(ptr, fmt, args);
    ptr = log_end(ptr);

    Puts(buffer, ptr - buffer);
    va_end(args);
}
