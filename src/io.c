/*
 * io.c - I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <io.h>
#include <vt100.h>
#include <ts7200.h>
#include <circular_buffer.h>

#define ON	1
#define	OFF	0

// IO buffers
struct out {
    char_buffer o;
    char buffer[4096];
};

struct in {
    char_buffer i;
    char buffer[16];
};

// buffer control
static struct out vt_out;
static struct in vt_in;


void uart_init() {

    // initialize the buffers
    cbuf_init(&vt_out.o, 4096, vt_out.buffer);
    cbuf_init(&vt_in.i,    16, vt_in.buffer);

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
    *line = *line & ~FEN_MASK;
}

void vt_write() {
    const int* const flags = (int*)( UART2_BASE + UART_FLAG_OFFSET);
    char* const       data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    if (cbuf_can_consume(&vt_out.o) && !(*flags & (TXFF_MASK|CTS_MASK)))
	*data = cbuf_consume(&vt_out.o);
}

void vt_flush() {
    while (cbuf_can_consume(&vt_out.o))
	vt_write();
}

void vt_read() {
    const int*  const flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    const char* const data  = (char*)(UART2_BASE + UART_DATA_OFFSET);

    if (*flags & RXFF_MASK)
	cbuf_produce(&vt_in.i, *data);
}

bool vt_can_get(void) {
    return cbuf_can_consume(&vt_in.i);
}

int vt_getc(void) {
    return cbuf_consume(&vt_in.i);
}

static void kprintf_num_simple(const uint32 num) {

    // build the string in reverse order to be used like a stack
    size     i = 0;
    uint32 nom = num;
    uint8  nums[10];

    for (; i < 10; i++) {
	nums[i] = (nom % 10);
	nom    -= nums[i];
	nom    /= 10;
    }

    // pop the stack contents, printing if we need to
    bool started = false;
    for (i = 9; i < 128; i--)
	if (nums[i] || started) {
	    started = true;
	    kprintf_char(nums[i] + '0');
	}
}

void kprintf_int(const int32 num) {

    // handle the 0 case specially
    if (num == 0) {
	kprintf_char('0');
	return;
    }

    // turn the negative case into a positive case
    if (num < 0) {
	kprintf_char('-');
	kprintf_num_simple((const uint32)-num);
    }
    else {
	kprintf_num_simple((const uint32)num);
    }
}

void kprintf_uint(const uint32 num) {

    // handle the 0 case now
    if (num == 0) {
	kprintf_char('0');
	return;
    }

    kprintf_num_simple(num);
}

void kprintf_hex(const uint32 num) {

    kprintf_char('0');
    kprintf_char('x');

    if (num == 0) {
	kprintf_char('0');
	return;
    }

    // build the string in reverse order to be used like a stack
    size  i = 0;
    uint8 nums[8];
    uint8 nom = num;

    for (; i < 8; i++) {
	nums[i] = nom & 0xf; // take the lower four bits
	nom     = nom >> 4;  // shift last three off the end
    }

    // pop the stack contents, printing if we need to
    bool started = false;
    for (i = 7; i < 128; i--) {
	if (nums[i] || started) {
	    started = true;
	    if (nums[i] > 9)
		kprintf_char((nums[i] - 10) + 'A');
	    else
		kprintf_char(nums[i] + '0');
	}
    }
}

void kprintf_ptr(const void* const pointer) {

    kprintf_char('0');
    kprintf_char('x');

    // build the string in reverse order to be used like a stack
    size i = 0;
    uint8 nums[8];
    uint32 ptr = (uint32)pointer;

    for (; i < 8; i++) {
	nums[i] = ptr & 0xf; // take the lower four bits
	ptr     = ptr >> 4;  // shift last four off the end
    }

    // pop the stack contents, printing everything
    for (i = 7; i < 128; i--) {
	if (nums[i] > 9)
	    kprintf_char((nums[i] - 10) + 'A');
	else
	    kprintf_char(nums[i] + '0');
    }
}

void kprintf_string(const char* str) {
    while (str[0])
	kprintf_char(*str++);
}

void kprintf_char(const char c) {
    cbuf_produce(&vt_out.o, c);
}

void kprintf(const char* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kprintf_va(fmt, args);
    va_end(args);
}

void kprintf_va(const char* const fmt, va_list args) {

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
		kprintf_char((const char)va_arg(args, int));
		break;
	    case '%':
		kprintf_char('%');
		break;
	    default:
		kprintf("Unknown format type `%%%c`", token);
	    }
	    index += 2;
	}
	else {
	    kprintf_char(token);
	    index++;
	}
    }
}
