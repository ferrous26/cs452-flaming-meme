/*
 * io.c - I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <io.h>
#include <debug.h>
#include <ts7200.h>
#include <kernel.h>
#include <syscall.h>

#define NOP(count) for (volatile uint _cnt = 0; _cnt < (count>>2)+1; _cnt++)

// ASCII byte that tells VT100 to stop transmitting
// #define XOFF  17

// ASCII byte that tells VT100 to start transmitting (only send after XOFF)
// #define XON   19

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
    *ctlr = UARTEN_MASK | MSIEN_MASK;
}

inline static void uart_drain(const uint base) {
    volatile int* const data = (int*)(base + UART_DATA_OFFSET);
    for (int i = UART_FIFO_SIZE; i; i--) *data;
}

void uart_init() {
    uart_setoptions(UART1_BASE, 0xBF, 0);
    uart_setoptions(UART2_BASE, 0x03, 1);
    NOP(55);

    uart_initirq(UART1_BASE);
    uart_initirq(UART2_BASE);

    // also enable the receive interrupts for UART1
    int* const ctlr = (int*)(UART1_BASE + UART_CTLR_OFFSET);
    *ctlr |= RTIEN_MASK | RIEN_MASK;

    NOP(55);

    // want to clear the error registers since otehr people might
    // have set them off
    volatile int* volatile rsr1 = (int*)(UART1_BASE + UART_RSR_OFFSET);
    *rsr1 = (int)rsr1;

    volatile int* volatile rsr2 = (int*)(UART2_BASE + UART_RSR_OFFSET);
    *rsr2 = (int)rsr2;

    uart_drain(UART1_BASE);
    uart_drain(UART2_BASE);
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


#ifdef ASSERT
static inline void uart_rsr_check(int base) {
    volatile int* const rsr = (int*)(base + UART_RSR_OFFSET);
    assert(!(*rsr & 0xf), "UART %p has had an error %p", base, *rsr);
}
#else
#define uart_rsr_check(...)
#endif

static void __attribute__ ((noreturn, noinline)) magic_sysreq() {
    Abort(__FILE__, 0, "Magic SysRq key pressed");
}

void irq_uart2_recv() {
    uart_rsr_check(UART2_BASE);

    volatile int*  const flag = (int*) (UART2_BASE + UART_FLAG_OFFSET);
    volatile char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    volatile int*  const ctlr = (int*) (UART2_BASE + UART_CTLR_OFFSET);

    assert(!(*flag & RXFE_MASK), "UART2 Had An Empty Recv");

    task* const t = int_queue[UART2_RECV];
    int_queue[UART2_RECV] = NULL;

    //assert(t, "Interrupts were on when we were not waiting...");

    if (t) {
        kreq_event* const req = (kreq_event*) t->sp[1];

        char c = *data;
        if (c == '`') magic_sysreq();
	req->event[0] = c;

        int i;
        for (i = 1; !(*flag & RXFE_MASK) && i < req->eventlen; i++) {
            req->event[i] = *data;
        }

        t->sp[0] = i;
	*ctlr &= ~(RTIS_MASK | RIS_MASK);
        scheduler_reschedule(t);

        return;
    }

    // we aren't necessarily dropping the byte,
    // we are missing the interrupt, but if we mask the interrupt
    // and just return to task handling, the notifier might come
    // back fast enough to deal with the data; so we should only
    // fire this off if we have actually lost a byte
    if (*flag & RXFF_MASK) ABORT("UART2 dropped %c", *data);
    // however, we should be disabling interrupts when we are
    // not waiting for input, so we should not be here if the
    // notifier is not waiting, something is wrong with the way
    // that we mask interrupts
}

void irq_uart2_send() {
    uart_rsr_check(UART2_BASE);

    volatile char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);

    task* t = int_queue[UART2_SEND];
    int_queue[UART2_SEND] = NULL;

    assert(t != NULL, "UART2 SEND INTERRUPT WITHOUT SENDER!");
    kreq_event* const req = (kreq_event*)t->sp[1];

    const int count = 8 < req->eventlen ? 8 : req->eventlen;
    for (int i = 0; i < count; i++) {
        *data = req->event[i];
    }

    t->sp[0] = count;

    // disable the interrupt now that we have sent out a full block
    int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;

    scheduler_reschedule(t);
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
    volatile char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);

    task* t = int_queue[UART1_RECV];
    int_queue[UART1_RECV] = NULL;

    if (t != NULL) {
        kreq_event* const req_space = (kreq_event*) t->sp[1];
        req_space->event[0] = *data;
        t->sp[0] = 1;

        scheduler_reschedule(t);
        return;
    }

    assert(false, "UART1 dropped char %d", *data);
}

void irq_uart1_send() {
    uart_rsr_check(UART1_BASE);

    volatile char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);

    task* t = int_queue[UART1_SEND];
    int_queue[UART1_SEND] = NULL;

    assert(t != NULL, "UART1 SEND INTERRUPT WITHOUT SENDER!");
    kreq_event* const req_space = (kreq_event*) t->sp[1];
    *data = req_space->event[0];
    t->sp[0] = 1;

    int* const ctlr = (int*)(UART1_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;
    scheduler_reschedule(t);
}

void irq_uart1() {
    uart_rsr_check(UART1_BASE);

    int* const intr = (int*)(UART1_BASE + UART_INTR_OFFSET);
    int* const flag = (int*)(UART1_BASE + UART_FLAG_OFFSET);

    assert(*intr & MIS_MASK,     "UART1 in general without modem");
    assert(!(*intr & RTIS_MASK), "UART1 got a receive timeout");

    *intr = (int)intr;

    task* t;
    if (*flag & CTS_MASK) {
        t = int_queue[UART1_CTS];
        int_queue[UART1_CTS] = NULL;
    } else {
        t = int_queue[UART1_DOWN];
        int_queue[UART1_DOWN] = NULL;
    }

    if (t) scheduler_reschedule(t);
}
