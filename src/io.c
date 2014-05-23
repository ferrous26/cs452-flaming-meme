/*
 * io.c - I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#ifdef PI
#include <pi.h>
#else
#include <ts7200.h>
#endif

#include <io.h>
#include <vt100.h>
#include <circular_buffer.h>

#define ON	1
#define	OFF	0

// IO buffers
struct out {
    char_buffer o;
    char buffer[2048];
};

struct in {
    char_buffer i;
    char buffer[128];
};

// buffer control
static struct out vt_out;
static struct in vt_in;

#if PI
static inline void mmio_write(size reg, size data) {
    size* ptr = (size*)reg;
    asm volatile("str %[data], [%[reg]]"
		 :
		 : [reg]"r"(ptr), [data]"r"(data));
}

static inline size mmio_read(size reg) {
    size* ptr = (size*)reg;
    size data;
    asm volatile("ldr %[data], [%[reg]]"
		 : [data]"=r"(data) : [reg]"r"(ptr));
    return data;
}

/*
 * delay function
 * int32_t delay: number of cycles to delay
 *
 * This just loops <delay> times in a way that the compiler
 * wont optimize away.
 */
static void delay(int32 count) {
    asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
		 : : [count]"r"(count) : "cc");
}
#endif


void uart_init() {
#ifdef PI
    /* Reference material:
     * http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
     * Chapter 13: UART
     */

    // Disable UART0.
    mmio_write(UART0_CR, 0x00000000);
    // Setup the GPIO pin 14 && 15.

    // Disable pull up/down for all GPIO pins & delay for 150 cycles.
    mmio_write(GPPUD, 0x00000000);
    delay(150);

    // Disable pull up/down for pin 14,15 & delay for 150 cycles.
    mmio_write(GPPUDCLK0, (1 << 14) | (1 << 15));
    delay(150);

    // Write 0 to GPPUDCLK0 to make it take effect.
    mmio_write(GPPUDCLK0, 0x00000000);

    // Clear pending interrupts.
    mmio_write(UART0_ICR, 0x7FF);

    // Set integer & fractional part of baud rate.
    // Divider = UART_CLOCK/(16 * Baud)
    // Fraction part register = (Fractional part * 64) + 0.5
    // UART_CLOCK = 3000000; Baud = 115200.

    // Divider = 3000000/(16 * 115200) = 1.627 = ~1.
    // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
    mmio_write(UART0_IBRD, 1);
    mmio_write(UART0_FBRD, 40);

    // Enable FIFO & 8 bit data transmissio (1 stop bit, no parity).
    mmio_write(UART0_LCRH, (1 << 4) | (1 << 5) | (1 << 6));

    // Mask all interrupts.
    mmio_write(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) |
	       (1 << 6) | (1 << 7) | (1 << 8) |
	       (1 << 9) | (1 << 10));

    // Enable UART0, receive & transfer part of UART.
    mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9));

#else

    // initialize the buffers
    cbuf_init(&vt_out.o, 2048, vt_out.buffer);
    cbuf_init(&vt_in.i,   128, vt_in.buffer);

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
#endif
}


void vt_write() {
#if PI
    if (!(mmio_read(UART0_FR) & (1 << 5)))
	mmio_write(UART0_DR, cbuf_consume(&vt_out.o));
#else
    const int* const flags = (int*)( UART2_BASE + UART_FLAG_OFFSET);
    char* const       data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    if (cbuf_can_consume(&vt_out.o) && !(*flags & (TXFF_MASK|CTS_MASK)))
	*data = cbuf_consume(&vt_out.o);
#endif
}

void vt_flush() {
    while (cbuf_can_consume(&vt_out.o))
	vt_write();
}

void vt_read() {
#if PI
    if (!(mmio_read(UART0_FR) & (1 << 4)))
	cbuf_produce(&vt_in.i, mmio_read(UART0_DR));
#else
    const int*  const flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    const char* const data  = (char*)(UART2_BASE + UART_DATA_OFFSET);

    if (*flags & RXFF_MASK)
	cbuf_produce(&vt_in.i, *data);
#endif
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
