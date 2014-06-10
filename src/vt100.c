
#include <io.h>
#include <syscall.h>

#include <vt100.h>


static inline void vt_reset_scroll_region();
static inline void vt_set_scroll_region(uint start, uint end);
static inline void vt_save_cursor();
static inline void vt_restore_cursor();

static uint log_count = 0;

void vt_init() {
    vt_clear_screen();
    vt_hide_cursor();

    vt_set_scroll_region(LOG_HOME, 80);
    vt_goto(LOG_HOME, 0);
    vt_save_cursor();

    log_count = 0;
}

void vt_deinit() {
    vt_reset_scroll_region();
    vt_unhide_cursor();
    vt_flush();
}

static inline void vt_esc() {
    kprintf_char((char)0x1b); // ESC
    kprintf_char('[');
}

void vt_clear_screen() {
    vt_esc();
    kprintf_char('2');
    kprintf_char('J');
}

void vt_goto_home() {
    vt_esc();
    kprintf_char('H');
}

static void vt_set_location(const uint first, const uint second) {
    const char first_high  = (char)(first / 10) + 48;
    const char first_low   = (char)(first % 10) + 48;
    const char second_high = (char)(second / 10) + 48;
    const char second_low  = (char)(second % 10) + 48;

    vt_esc();
    if (first_high > 48) kprintf_char(first_high);
    kprintf_char(first_low);
    kprintf_char(';');
    if (second_high > 48) kprintf_char(second_high);
    kprintf_char(second_low);
}

void vt_goto(const uint8 row, const uint8 column) {
    vt_set_location(row, column);
    kprintf_char('H');
}

void vt_hide_cursor() {
    vt_esc();
    kprintf_char('?');
    kprintf_char('2');
    kprintf_char('5');
    kprintf_char('l');
}

void vt_unhide_cursor() {
    vt_esc();
    kprintf_char('?');
    kprintf_char('2');
    kprintf_char('5');
    kprintf_char('h');
}

void vt_kill_line() {
    vt_esc();
    kprintf_char('K');
}

void vt_reverse_kill_line() {
    vt_esc();
    kprintf_char('1');
    kprintf_char('K');
}

static inline void vt_set_scroll_region(uint start, uint end) {
    vt_set_location(start, end);
    kprintf_char('r');
}

static inline void vt_reset_scroll_region() {
    vt_esc();
    kprintf_char('r');
}

void vt_scroll_up() {
    kprintf_char((char)0x1b); // ESC
    kprintf_char('M');
}

void vt_scroll_down() {
    kprintf_char((char)0x1b); // ESC
    kprintf_char('D');
}

void vt_colour_reset() {
    vt_esc();
    kprintf_char('0');
    kprintf_char('m');
}

void vt_colour(const colour c) {
    vt_esc();
    kprintf_char('3');
    kprintf_char('0' + c);
    kprintf_char('m');
}

static inline void vt_save_cursor() {
    kprintf_char((char)0x1b); // ESC
    kprintf_char('7');
}

static inline void vt_restore_cursor() {
    kprintf_char((char)0x1b); // ESC
    kprintf_char('8');
}

void vt_log_start() {
    log_count++;
    vt_restore_cursor();
    kprintf_uint(log_count);
    kprintf_string(": ");
}

void vt_log_end() {
    kprintf_string("\n"); // do not need to send \r
    vt_save_cursor();
}

void vt_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vt_log_start();
    kprintf_va(fmt, args);
    vt_log_end();

    va_end(args);
}

char vt_waitget() {
    while( !vt_can_get() ) {
        vt_read();
        Pass();
    }
    return vt_getc();
}
