/*
 * io.c - I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <io.h>
#include <debug.h>

#include <ts7200.h>
#include <scheduler.h>

#include <circular_buffer.h>
#include <syscall.h>

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

#define NOP(count) for(volatile uint _cnt = 0; _cnt < (count>>3)+1; _cnt++)

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
    int* const baud_high = (int*)(UART2_BASE + UART_LCRM_OFFSET);
    int* const baud_low = (int*)(UART2_BASE + UART_LCRL_OFFSET);
    *baud_high = 0x0;
    *baud_low  = 0x3;

    NOP(55);

    // disable fifo for vt100
    int* const lcrh = (int*)(UART2_BASE + UART_LCRH_OFFSET);
    *lcrh &= ~FEN_MASK;

    NOP(55);

    int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
    *ctlr |= RIEN_MASK|RTIEN_MASK|UARTEN_MASK;
}

void vt_write() {
    const int* const flags = (int*) (UART2_BASE + UART_FLAG_OFFSET);
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

char vt_getc(void) {
    return cbuf_consume(&vt_in.i);
}

char vt_waitget() {
    while (!vt_can_get()) {
        vt_read();
        Pass();
    }
    return vt_getc();
}

void kprintf_string(const char* str, size strlen) {
    while (strlen--)
	cbuf_produce(&vt_out.o, *str++);
}

static uint __attribute__ ((const)) uart_get_interrupt(const uint irqr) {
    if (irqr & RTIS_MASK) return 1;
    if (irqr & TIS_MASK)  return 2;
    if (irqr & RIS_MASK)  return 3;
    if (irqr & MIS_MASK)  return 4;
    return 0;
}

// must be a seperate function so we can use the
// receive register shortcut provided by the vec
// controller
void irq_uart2_recv() {
    const char* const uart2_data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART2_RECV];
    int_queue[UART2_RECV] = NULL;

    if (t != NULL) {
        //TODO: give data to task....
        scheduler_schedule(t);
    }

    kprintf_string("dropped char: ", 14);
    cbuf_produce(&vt_out.o, *uart2_data);
    vt_flush();
}

void irq_uart2_send() {
    volatile char* const uart2_data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART2_SEND];
    int_queue[UART2_SEND] = NULL;

    assert(t != NULL, "UART2 SEND INTERRUPT WITHOUT SENDER!");
    // TODO: send data....
    *uart2_data = 'c';


    int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;
    scheduler_schedule(t);
}

void irq_uart2() {
    uint* const uart2_irqr = (uint*)(UART2_BASE + UART_INTR_OFFSET);
    const uint irq = uart_get_interrupt(*uart2_irqr);

    switch (irq) {
    case 1:     // RECEIVE TIMEOUT
    case 3:     // RECEIVE
        irq_uart2_recv();
        break;
    case 2:     // TRANSMIT
        irq_uart2_send();
        break;
    case 4:     // MODEM
        kprintf_string("MODEM!", 6);
        vt_flush();
        *uart2_irqr = (uint)uart2_irqr;
        break;
    }
}
