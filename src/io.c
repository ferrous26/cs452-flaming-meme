/*
 * io.c - I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <io.h>
#include <ts7200.h>
#include <circular_buffer.h>

#define ON	1
#define	OFF	0

// IO buffers
static char vt_outs[2048];
static char vt_ins[128];

// buffer control
static char_buffer vt_out;
static char_buffer vt_in;

// keep track of where to put debug messages
static uint32 error_line  = 0;
static uint32 error_count = 0;

static inline void kprintf_va(const char* const fmt, va_list args);


void uart_init() {

    // initialize the buffers
    cbuf_init(&vt_out, 2048, vt_outs);
    cbuf_init(&vt_in,   128,  vt_ins);

    error_line  = DEBUG_HOME - 1;
    error_count = 0;

    /**
     * BAUDDIV = (F_uartclk / (16 * BAUD_RATE)) - 1
     * http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/EP93xx_Users_Guide_UM1.pdf
     * page 16-8
     *
     * F_uartclk = 7.3728 MHz
     * http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/ts-7000_ep9301-ug.pdf
     * page 82
     *
     * Therefore:
     * BAUDDIV = 191 = 0xbf
     */

    // speed for vt100
    int* const high = (int*)(UART2_BASE + UART_LCRM_OFFSET);
    int* const  low = (int*)(UART2_BASE + UART_LCRL_OFFSET);
    *high = 0x0;
    *low  = 0x3;

    // disable fifo for vt100
    int* const line = (int*)(UART2_BASE + UART_LCRH_OFFSET);
    *line = *line & ~FEN_MASK; // TODO: should this not be the other way around?
}

void debug_message(const char* const msg, ...) {

    error_line++;
    error_count++;
    if (error_line == DEBUG_END) error_line = DEBUG_HOME; // reset

    vt_goto(error_line, 1);
    vt_kill_line();
    kprintf_uint(error_count);
    kprintf_string(": ");

    va_list args;
    va_start(args, msg);
    kprintf_va(msg, args);
    va_end(args);
}

void vt_write() {

    const int* const flags = (int*)( UART2_BASE + UART_FLAG_OFFSET);
    char* const       data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    if (cbuf_can_consume(&vt_out) && !(*flags & (TXFF_MASK|CTS_MASK)))
	*data = cbuf_consume(&vt_out);
}

void vt_bwputc(const char c) {

    const int* flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    int* const data  = (int*)(UART2_BASE + UART_DATA_OFFSET);

    while (*flags & TXFF_MASK);
    *data = c;
}

void vt_putc(const char c) {
    cbuf_produce(&vt_out, c);
}

void vt_read() {

    const int*  const flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    const char* const data  = (char*)(UART2_BASE + UART_DATA_OFFSET);

    if (*flags & RXFF_MASK)
	cbuf_produce(&vt_in, *data);
}

int vt_can_get() {
    return cbuf_can_consume(&vt_in);
}

int vt_getc() {
    return cbuf_consume(&vt_in);
}

void vt_esc() {
    vt_putc((char)0x1b); // ESC
    vt_putc('[');
}

void vt_blank() {
    vt_esc();
    vt_putc('2');
    vt_putc('J');
}

void vt_bwblank() {
    vt_bwputc((char)0x1b);
    vt_bwputc('[');
    vt_bwputc('2');
    vt_bwputc('J');
}

void vt_home() {
    vt_esc();
    vt_putc('H');
}

void vt_hide() {
    vt_esc();
    vt_putc('?');
    vt_putc('2');
    vt_putc('5');
    vt_putc('l');
}

void vt_kill_line() {
    vt_esc();
    vt_putc('K');
}

void vt_goto(const uint8 row, const uint8 column) {
    const char row_high = row / 10;
    const char row_low  = row % 10;
    const char col_high = column / 10;
    const char col_low  = column % 10;

    vt_esc();
    if (row_high) vt_putc(row_high + '0');
    vt_putc(row_low + '0');
    vt_putc(';');
    if (col_high) vt_putc(col_high + '0');
    vt_putc(col_low + '0');
    vt_putc('H');
}

static void kprintf_num_stack(const uint8* nums, const size stack_size) {

    bool started = false;
    size i;

    // pop the stack contents, printing if we need to
    for (i = (stack_size - 1); i > 0; i--)
	if (nums[i] || started) {
	    started = true;
	    vt_putc(nums[i] + '0');
	}
}

static void kprintf_num_simple(const uint32 num) {

    // build the string in reverse order to be used like a stack
    size i;
    uint8  nums[16];
    uint32 nom = num;

    for (i = 1; i < 16; i++) {
	nums[i] = (nom % 10);
	nom    -= nums[i];
	nom    /= 10;
    }

    kprintf_num_stack(nums, 16);
}

void kprintf_int(const int32 num) {

    // handle the 0 case specially
    if (num == 0) {
	vt_putc('0');
	return;
    }

    // turn the negative case into a positive case
    if (num < 0) {
	vt_putc('-');
	kprintf_num_simple((const uint32)-num);
    }
    else {
	kprintf_num_simple((const uint32)num);
    }
}

void kprintf_uint(const uint32 num) {

    // handle the 0 case now
    if (num == 0) {
	vt_putc('0');
	return;
    }

    kprintf_num_simple(num);
}

void kprintf_hex(const uint32 num) {

    vt_putc('0');
    vt_putc('x');

    if (num == 0) {
	vt_putc('0');
	return;
    }

    // build the string in reverse order to be used like a stack
    size i;
    uint8 nums[10];
    uint8 nom = num;

    for (i = 1; i < 10; i++) {
	nums[i] = nom & 0xf; // take the lower four bits
	nom     = nom >> 4;  // shift last three off the end
    }

    // pop the stack contents, printing if we need to
    bool started = false;
    for (i = 9; i > 0; i--) {
	if (nums[i] || started) {
	    started = true;
	    if (nums[i] > 9)
		vt_putc((nums[i] - 10) + 'A');
	    else
		vt_putc(nums[i] + '0');
	}
    }
}

void kprintf_ptr(const void* const ptr) {

    vt_putc('0');
    vt_putc('x');

    // build the string in reverse order to be used like a stack
    size i;
    uint8 nums[8];
    uint32 potr = (uint32)ptr;

    for (i = 0; i < 8; i++) {
	nums[i] = potr & 0xf; // take the lower four bits
	potr    = potr >> 4;  // shift last three off the end
    }

    // pop the stack contents, printing everything
    for (i = 8; i > 0; i--) {
	if (nums[i-1] > 9)
	    vt_putc((nums[i-1] - 10) + 'A');
	else
	    vt_putc(nums[i-1] + '0');
    }
}

void kprintf_string(const char* str) {
    while (str[0])
	vt_putc(*str++);
}

void kprintf_bwstring(const char* str) {
    while (str[0])
	vt_bwputc(*str++);
}

void vt_colour_reset() {
    vt_esc();
    vt_putc('0');
    vt_putc('m');
}

void vt_colour(const colour c) {
    vt_esc();
    vt_putc('3');
    vt_putc('0' + c);
    vt_putc('m');
}

void kprintf(const char* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kprintf_va(fmt, args);
    va_end(args);
}

static inline void kprintf_va(const char* const fmt, va_list args) {

    size index = 0;
    while (1) {
	char token = fmt[index];
	if (!token) break;

	if (token == '%') {
	    token = fmt[index + 1];
	    switch (token) {
	    case 'd': // decimal (signed)
		kprintf_int(va_arg(args, int32));
		break;
	    case 'u': // decimal (unsigned)
		kprintf_uint(va_arg(args, uint32));
		break;
	    case 'x': // hex (unsigned)
		kprintf_hex(va_arg(args, uint32));
		break;
	    case 'p': // pointer
		kprintf_ptr(va_arg(args, void*));
		break;
	    case 's': // string
		// recursion feels like it should be the correct solution
		// but it would be a security flaw (and also less efficient)
		kprintf_string(va_arg(args, char*));
		break;
	    case 'c': // character
		vt_putc((const char)va_arg(args, int));
		break;
	    case '%':
		vt_putc('%');
		break;
	    default:
		kprintf("Unknown format type `%%%c`", token);
	    }
	    index += 2;
	}
	else {
	    vt_putc(token);
	    index++;
	}
    }
}
