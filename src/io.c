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

#define NOP(count) for(volatile uint _cnt = 0; _cnt < (count>>2)+1; _cnt++)

inline static void uart_setoptions(int base, int speed, int fifo) {
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
    volatile int* const lcrl = (int*)(base + UART_LCRL_OFFSET);
    volatile int* const lcrm = (int*)(base + UART_LCRM_OFFSET);
    volatile int* const lcrh = (int*)(base + UART_LCRH_OFFSET);

    *lcrl = speed;
    *lcrm = 0x0;

    if (fifo) {
        *lcrh |= FEN_MASK;
    } else {
        *lcrh &= ~FEN_MASK;
    }
}

inline static void uart_initirq(int base) {
    int* const ctlr = (int*)(base + UART_CTLR_OFFSET);
    *ctlr = RIEN_MASK | RTIEN_MASK | UARTEN_MASK | MSIEN_MASK;
}

void uart_init() {
    uart_setoptions(UART1_BASE, 0xBF, 0);
    uart_setoptions(UART2_BASE, 0x03, 1);
    
    NOP(55);

    uart_initirq(UART1_BASE);
    uart_initirq(UART2_BASE);
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

void irq_uart2_recv() {
    volatile int* const flag = (int*) (UART2_BASE + UART_FLAG_OFFSET);
    const char* const   data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    
    task* t = int_queue[UART2_RECV];
    int_queue[UART2_RECV] = NULL;

    if (t) {
        kreq_event* const req = (kreq_event*) t->sp[1];

        int i;
        for (i = 0; !(*flag & RXFE_MASK) && i < req->eventlen; i++) {
            req->event[i] = *data;
        }

        assert(i > 0, "UART2 Had An Empty Send");
        t->sp[0] = i;

        scheduler_schedule(t);
        return;
    }

    kprintf(32, "\nUART2 dropped %c\n", *data);
}

void irq_uart2_send() {
    volatile int*  const flag = (int*) (UART2_BASE + UART_FLAG_OFFSET);
    volatile char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    
    task* t = int_queue[UART2_SEND];
    int_queue[UART2_SEND] = NULL;

    kassert(t != NULL, "UART2 SEND INTERRUPT WITHOUT SENDER!");
    kreq_event* const req = (kreq_event*)t->sp[1];

    int i;
    for(i = 0; !(*flag & TXFF_MASK) && i < req->eventlen ;i++) {
        *data = req->event[i];
    }

    assert(i > 0, "UART2 Had An Empty Send");
    t->sp[0] = i;

    int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;
    scheduler_schedule(t);
}

void irq_uart2() {
    uint* const intr = (uint*)(UART2_BASE + UART_INTR_OFFSET);

    UNUSED(intr);
    assert(*intr & RTIS_MASK,   "UART2 in general without timeout");
    assert(!(*intr & MIS_MASK), "UART2 got a modem interrupt");

    irq_uart2_recv();
}

void irq_uart1_recv() {
    const char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART1_RECV];
    int_queue[UART1_RECV] = NULL;

    if (t != NULL) {
        kreq_event* const req_space = (kreq_event*) t->sp[1];
        req_space->event[0] = *data;
        t->sp[0] = 1;

        scheduler_schedule(t);
        return;
    }

    char mesg[] = "UART1 dropped char %d\n";
    kprintf(sizeof(mesg)+6, mesg, *data);
}

void irq_uart1_send() {
    volatile char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART1_SEND];
    int_queue[UART1_SEND] = NULL;

    assert(t != NULL, "UART1 SEND INTERRUPT WITHOUT SENDER!");
    kreq_event* const req_space = (kreq_event*) t->sp[1];
    *data = req_space->event[0];
    t->sp[0] = 1;

    int* const ctlr = (int*)(UART1_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;
    scheduler_schedule(t);
}

void irq_uart1() {
    uint* const intr = (uint*)(UART1_BASE + UART_INTR_OFFSET);
    assert(*intr & MIS_MASK,     "UART1 in general without modem");
    assert(!(*intr & RTIS_MASK), "UART1 got a receive timeout");
    
    *intr = (uint)intr;
    
    task* t = int_queue[UART1_MODM];
    if( t != NULL ) {
        int_queue[UART1_MODM] = NULL;
        scheduler_schedule(t);
    }
}

