#include <vt100.h>
#include <io.h>

void vt_init() {
    vt_blank();
    vt_hide();
}

void vt_esc() {
    kprintf_char((char)0x1b); // ESC
    kprintf_char('[');
}

void vt_blank() {
    vt_esc();
    kprintf_char('2');
    kprintf_char('J');
}

void vt_home() {
    vt_esc();
    kprintf_char('H');
}

void vt_hide() {
    vt_esc();
    kprintf_char('?');
    kprintf_char('2');
    kprintf_char('5');
    kprintf_char('l');
}

void vt_kill_line() {
    vt_esc();
    kprintf_char('K');
}

void vt_goto(const uint8 row, const uint8 column) {
    const char row_high = (row / 10) + 48;
    const char row_low  = (row % 10) + 48;
    const char col_high = (column / 10) + 48;
    const char col_low  = (column % 10) + 48;

    vt_esc();
    if (row_high > 48) kprintf_char(row_high);
    kprintf_char(row_low);
    kprintf_char(';');
    if (col_high > 48) kprintf_char(col_high);
    kprintf_char(col_low);
    kprintf_char('H');
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
