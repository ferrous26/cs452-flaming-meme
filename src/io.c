/*
 * io.c - I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <io.h>
#include <debug.h>
#include <vt100.h>

#include <ts7200.h>
#include <scheduler.h>

#include <circular_buffer.h>
#include <syscall.h>


#define NOP(count) for(volatile uint _cnt = 0; _cnt < (count>>3)+1; _cnt++)

void uart_init() {
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
    *ctlr = RIEN_MASK|RTIEN_MASK|UARTEN_MASK;
}

void uart2_bw_write(const char* string, uint length) {

    volatile const int* const flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    volatile char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    for (; length; length--) {
	while (*flags & (TXFF_MASK|CTS_MASK));
	*data = *string++;
    }
}

bool uart2_bw_can_read(void) {
    volatile const int* const flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    return (*flags & RXFF_MASK);
}

char uart2_bw_read() {
    volatile const char* const data  = (char*)(UART2_BASE + UART_DATA_OFFSET);
    return *data;
}

char uart2_bw_waitget() {
    while (!uart2_bw_can_read());
    return uart2_bw_read();
}

static inline uint __attribute__ ((const)) uart_get_interrupt(const uint irqr) {
    if (irqr & RTIS_MASK) return 1;
    if (irqr & TIS_MASK)  return 2;
    if (irqr & RIS_MASK)  return 3;
    if (irqr & MIS_MASK)  return 4;
    return 0;
}

// must be a seperate function so we can use the
// receive register shortcut provided by the vec
// controller
static void irq_uart2_recv() {
    const char* const uart2_data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART2_RECV];
    int_queue[UART2_RECV] = NULL;

    if (t) {
        kwait_req* const req_space = (kwait_req*) t->sp[1];
        req_space->event[0] = *uart2_data;
        t->sp[0] = 1;

        scheduler_schedule(t);
        return;
    }

    kprintf(16, "dropped char: %c", *uart2_data);
}

static void irq_uart2_send() {
    volatile char* const uart2_data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART2_SEND];
    int_queue[UART2_SEND] = NULL;

    kassert(t != NULL, "UART2 SEND INTERRUPT WITHOUT SENDER!");

    kwait_req* const req_space = (kwait_req*)t->sp[1];
    *uart2_data = req_space->event[0];
    t->sp[0] = 1;

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
        kprintf(6, "MODEM!");
        *uart2_irqr = (uint)uart2_irqr;
        break;
    }
}
