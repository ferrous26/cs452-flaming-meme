
#include <vt100.h>
#include <io.h>
#include <debug.h>
#include <tasks/term_server.h>
#include <tasks/clock_server.h>


static uint log_count = 0;

void vt_init() {
    char buffer[32];
    char* ptr = buffer;

    ptr = vt_clear_screen(ptr);
    ptr = vt_hide_cursor(ptr);

    ptr = vt_set_scroll_region(ptr, LOG_HOME, 80);
    ptr = vt_goto(ptr, LOG_HOME, 1);
    ptr = vt_save_cursor(ptr);

    uart2_bw_write(buffer, ptr - buffer);

    log_count = 0;
}

void vt_deinit() {
    char buffer[64];
    char* ptr = buffer;

    ptr = vt_reset_scroll_region(ptr);
    ptr = vt_unhide_cursor(ptr);
    ptr = sprintf_string(ptr, "\n\rSHUTTING DOWN");
    uart2_bw_write(buffer, ptr - buffer);
}


char* sprintf(char* buffer, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    buffer = sprintf_va(buffer,fmt, args);
    va_end(args);
    return buffer;
}

char* sprintf_va(char* buffer, const char* fmt, va_list args) {

    char token = *fmt;
    while (token) {
	if (token == '%') {
	    fmt++;
	    token = *fmt;

	    assert(token == '%' || token == 'c' || token == 'd' ||
		   token == 'p' || token == 's' || token == 'u' ||
		   token == 'x', "Unknown format type `%%%c`", token);

	    switch (token) {
	    case '%':
		buffer = sprintf_char(buffer, token);
		break;
	    case 'c': // character
		buffer = sprintf_char(buffer, (const char)va_arg(args, int));
		break;
	    case 'd': // decimal (signed)
		buffer = sprintf_int(buffer, va_arg(args, int32));
		break;
	    case 'p': // pointer
		buffer = sprintf_ptr(buffer, va_arg(args, void*));
		break;
	    case 's': // string
		// recursion feels like it should be the correct solution
		// but it would be a security flaw (and also less efficient)
		buffer = sprintf_string(buffer, va_arg(args, char*));
		break;
	    case 'u': // decimal (unsigned)
		buffer = sprintf_uint(buffer, va_arg(args, uint32));
		break;
	    case 'x': // hex (unsigned)
		buffer = sprintf_hex(buffer, va_arg(args, uint32));
		break;
	    }
	}
	else {
	    buffer = sprintf_char(buffer, token);
	}

	fmt++;
	token = *fmt;
    }

    return buffer;
}


static inline char* sprintf_num_simple(char* buffer, const uint32 num) {

    // build the string in reverse order to be used like a stack
    uint     i = 0;
    uint32 nom = num;
    uint8  noms[10];

    for (; i < 10; i++) {
	noms[i] = (uint8)(nom % 10);
	nom    -= noms[i];
	nom    /= 10;
    }

    // pop the stack contents, printing if we need to
    bool started = false;
    for (i = 9; i < 128; i--)
	if (noms[i] || started) {
	    started = true;
	    buffer = sprintf_char(buffer, '0' + noms[i]);
	}

    return buffer;
}

char* sprintf_int(char* buffer, const int32 num) {

    // handle the 0 case specially
    if (!num)
	return sprintf_char(buffer, '0');

    // turn the negative case into a positive case
    if (num < 0) {
	buffer = sprintf_char(buffer, '-');
	return sprintf_num_simple(buffer, (const uint32)-num);
    }

    return sprintf_num_simple(buffer, (const uint32)num);
}

char* sprintf_uint(char* buffer, const uint32 num) {

    // handle the 0 case now
    if (!num)
	return sprintf_char(buffer, '0');

    return sprintf_num_simple(buffer, num);
}

char* sprintf_hex(char* buffer, const uint32 num) {

    buffer = sprintf_char(buffer, '0');
    buffer = sprintf_char(buffer, 'x');

    if (!num)
	return sprintf_char(buffer, '0');

    // build the string in reverse order to be used like a stack
    uint  i = 0;
    uint8 noms[8];
    uint  nom = num;

    for (; i < 8; i++) {
	noms[i] = nom & 0xf; // take the lower four bits
	nom     = nom >> 4;  // shift last three off the end
    }

    // pop the stack contents, printing if we need to
    bool started = false;
    for (i = 7; i < 128; i--) {
	if (noms[i] || started) {
	    started = true;
	    if (noms[i] > 9)
		buffer = sprintf_char(buffer, (noms[i] - 10) + 'A');
	    else
		buffer = sprintf_char(buffer, noms[i] + '0');
	}
    }

    return buffer;
}

char* sprintf_ptr(char* buffer, const void* const pointer) {

    buffer = sprintf_char(buffer, '0');
    buffer = sprintf_char(buffer, 'x');

    // build the string in reverse order to be used like a stack
    uint i = 0;
    uint8 noms[8];
    uint32 ptr = (uint32)pointer;

    for (; i < 8; i++) {
	noms[i] = ptr & 0xf; // take the lower four bits
	ptr     = ptr >> 4;  // shift last four off the end
    }

    // pop the stack contents, printing everything
    for (i = 7; i < 128; i--) {
	if (noms[i] > 9)
	    buffer = sprintf_char(buffer, (noms[i] - 10) + 'A');
	else
	    buffer = sprintf_char(buffer, noms[i] + '0');
    }

    return buffer;
}

char* sprintf_string(char* buffer, const char* str) {
    while (*str)
	buffer = sprintf_char(buffer, *str++);
    return buffer;
}

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

char* clog_start(char* buffer) {
    buffer = vt_restore_cursor(buffer);
    return sprintf(buffer, "%u: ", ++log_count);
}

char* klog_start(char* buffer) {
    buffer = vt_restore_cursor(buffer);
    return sprintf(buffer, "kernel %u: ", ++log_count);
}

char* log_end(char* buffer) {
    buffer = sprintf_string(buffer, "\r\n");
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

void clog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[256];
    char* ptr = buffer;

    ptr = clog_start(ptr);
    ptr = sprintf_va(ptr, fmt, args);
    ptr = log_end(ptr);

    Puts(buffer, ptr - buffer);
    va_end(args);
}

void klog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[256];
    char* ptr = buffer;

    ptr = klog_start(ptr);
    ptr = sprintf_va(ptr, fmt, args);
    ptr = log_end(ptr);

    uart2_bw_write(buffer, ptr - buffer);
    va_end(args);
}
