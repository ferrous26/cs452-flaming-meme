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

#define NOP(count) for (volatile uint _cnt = 0; _cnt < (count>>2)+1; _cnt++)

inline static void uart_setoptions(const uint base,
				   const int speed,
				   const int fifo) {
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
     * BAUDDIV = 191 = 0xbf (for the train controller)
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

inline static void uart_initirq(const uint base) {
    int* const ctlr = (int*)(base + UART_CTLR_OFFSET);
    *ctlr = RIEN_MASK | RTIEN_MASK | UARTEN_MASK | MSIEN_MASK;
}

void uart_init() {
    uart_setoptions(UART1_BASE, 0xBF, 0);
    uart_setoptions(UART2_BASE, 0x03, 1);

    NOP(55);

    uart_initirq(UART1_BASE);
    uart_initirq(UART2_BASE);

    NOP(55);

    // want to clear the error registers since otehr people might
    // have set them off
    int* const rsr1 = (int*)(UART1_BASE + UART_RSR_OFFSET);
    *rsr1 = (int)rsr1;

    int* const rsr2 = (int*)(UART2_BASE + UART_RSR_OFFSET);
    *rsr2 = (int)rsr2;
}

void uart2_bw_write(const char* string, int length) {
    volatile const int* const flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    volatile char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    for (; length; length--) {
        while (*flags & TXFF_MASK);
        *data = *string++;
    }
}

bool uart2_bw_can_read(void) {
    volatile const int* const flags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    return (*flags & RXFF_MASK);
}

char uart2_bw_read() {
    volatile const char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    return *data;
}

char uart2_bw_waitget() {
    while (!uart2_bw_can_read());
    return uart2_bw_read();
}


#ifdef DEBUG
static void uart_rsr_check(int base) {
    volatile int* const rsr = (int*)(base + UART_RSR_OFFSET);
    assert(!(*rsr & 0xf), "UART %p has had an error %p", base, *rsr);
}
#else
#define uart_rsr_check(...)
#endif

void irq_uart2_recv() {
    uart_rsr_check(UART2_BASE);

    volatile int* const flag = (int*) (UART2_BASE + UART_FLAG_OFFSET);
    const char*   const data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    task* t = task_term_recv;
    task_term_recv = NULL;

    if (t) {
        kreq_event* const req = (kreq_event*) t->sp[1];


        int i;
        for (i = 0; !(*flag & RXFE_MASK) && i < req->eventlen; i++) {
            req->event[i] = *data;
        }

        assert(i > 0, "UART2 Had An Empty Recv");
        t->sp[0] = i;

        scheduler_schedule(t);
        return;
    }

    assert(false, "UART2 dropped %c", *data);
}

void irq_uart2_send() {
    uart_rsr_check(UART2_BASE);

    volatile char* data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    task* t = task_term_send;
    task_term_send = NULL;

    assert(t != NULL, "UART2 SEND INTERRUPT WITHOUT SENDER!");
    kreq_event* const req = (kreq_event*)t->sp[1];

#define UART_IRQ_FILL 8
    int i = 0;
    int j = req->eventlen < UART_IRQ_FILL ? req->eventlen : UART_IRQ_FILL;

    assert(j > 0, "UART2 Has Nothing To Send %d %d", j, req->eventlen);

    for(; i < j; i++)
        *data = req->event[i];

    t->sp[0] = i;

    // disable the interrupt now that we have sent out a full block
    int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;

    scheduler_schedule(t);
}

void irq_uart2() {
    uart_rsr_check(UART2_BASE);
    int* const intr = (int*)(UART2_BASE + UART_INTR_OFFSET);

    UNUSED(intr);
    assert(*intr & RTIS_MASK,   "UART2 in general without timeout");
    assert(!(*intr & MIS_MASK), "UART2 got a modem interrupt");

    irq_uart2_recv();
}

void irq_uart1_recv() {
    uart_rsr_check(UART1_BASE);

    const char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);
    task* t = task_train_recv;
    task_train_recv = NULL;

    if (t != NULL) {
        kreq_event* const req_space = (kreq_event*) t->sp[1];
        req_space->event[0] = *data;
        t->sp[0] = 1;

        scheduler_schedule(t);
        return;
    }

    assert(false, "UART1 dropped char %d", *data);
}

void irq_uart1_send() {
    uart_rsr_check(UART1_BASE);

    volatile char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);
    task* t = task_train_send;
    task_train_send = NULL;

    assert(t != NULL, "UART1 SEND INTERRUPT WITHOUT SENDER!");
    kreq_event* const req_space = (kreq_event*) t->sp[1];
    *data = req_space->event[0];
    t->sp[0] = 1;

    int* const ctlr = (int*)(UART1_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;
    scheduler_schedule(t);
}

void irq_uart1() {
    uart_rsr_check(UART1_BASE);

    int* const intr = (int*)(UART1_BASE + UART_INTR_OFFSET);
    assert(*intr & MIS_MASK,     "UART1 in general without modem");
    assert(!(*intr & RTIS_MASK), "UART1 got a receive timeout");

    *intr = (int)intr;

    task* t = task_train_modm;
    if (t != NULL) {
        task_train_modm = NULL;
        scheduler_schedule(t);
    }
}
