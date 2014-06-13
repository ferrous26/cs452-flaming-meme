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

inline static void uart_setspeed(int base, int speed) {
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
    int* const baud_high = (int*)(base + UART_LCRM_OFFSET);
    int* const baud_low =  (int*)(base + UART_LCRL_OFFSET);
    *baud_high = 0x0;
    *baud_low  = speed;
}

inline static void uart_initirq(int base) {
    int* const ctlr = (int*)(base + UART_CTLR_OFFSET);
    *ctlr = RIEN_MASK | RTIEN_MASK | UARTEN_MASK | MSIEN_MASK;
}

void uart_init() {
    uart_setspeed(UART1_BASE, 0xBF);
    uart_setspeed(UART2_BASE, 0x03);
    NOP(55);

    int* const lcrh1 = (int*)(UART1_BASE + UART_LCRH_OFFSET);
    *lcrh1 &= ~FEN_MASK;
    
    int* const lcrh2 = (int*)(UART2_BASE + UART_LCRH_OFFSET);
    *lcrh2 &= ~FEN_MASK;
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

static inline uint __attribute__ ((const)) uart_get_interrupt(const uint irqr) {
    if (irqr & RTIS_MASK) return 1;
    if (irqr & TIS_MASK)  return 2;
    if (irqr & RIS_MASK)  return 3;
    if (irqr & MIS_MASK)  return 4;
    return 0;
}

void irq_uart2_recv() {
    const char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART2_RECV];
    int_queue[UART2_RECV] = NULL;

    if (t) {
        kwait_req* const req_space = (kwait_req*) t->sp[1];
        req_space->event[0] = *data;
        t->sp[0] = 1;

        scheduler_schedule(t);
        return;
    }
    
    kprintf(32, "UART2 dropped char %c", *data);
}

void irq_uart2_send() {
    volatile char* const data = (char*)(UART2_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART2_SEND];
    int_queue[UART2_SEND] = NULL;

    kassert(t != NULL, "UART2 SEND INTERRUPT WITHOUT SENDER!");
    kwait_req* const req_space = (kwait_req*) t->sp[1];
    *data = req_space->event[0];
    t->sp[0] = 1;

    int* const ctlr = (int*)(UART2_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;
    scheduler_schedule(t);
}

void irq_uart2() {
    uint* const irqr = (uint*)(UART2_BASE + UART_INTR_OFFSET);
    const uint irq = uart_get_interrupt(*irqr);

    switch (irq) {
    case 1:     // RECEIVE TIMEOUT
    case 3:     // RECEIVE
        irq_uart2_recv();
        break;
    case 2:     // TRANSMIT
        irq_uart2_send();
        break;
    case 4:     // MODEM
        *irqr = (uint)irqr;
        assert(false, "UART2 should not get modem interrupts!");
        break;
    }
}

void irq_uart1_recv() {
    const char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART1_RECV];
    int_queue[UART1_RECV] = NULL;

    if (t != NULL) {
        kwait_req* const req_space = (kwait_req*) t->sp[1];
        req_space->event[0] = *data;
        t->sp[0] = 1;

        scheduler_schedule(t);
        return;
    }

    kprintf(32, "UART1 dropped char %d\n", *data);
}

void irq_uart1_send() {
    volatile char* const data = (char*)(UART1_BASE + UART_DATA_OFFSET);
    task* t = int_queue[UART1_SEND];
    int_queue[UART1_SEND] = NULL;

    assert(t != NULL, "UART1 SEND INTERRUPT WITHOUT SENDER!");
    kwait_req* const req_space = (kwait_req*) t->sp[1];
    *data = req_space->event[0];
    t->sp[0] = 1;

    int* const ctlr = (int*)(UART1_BASE + UART_CTLR_OFFSET);
    *ctlr &= ~TIEN_MASK;
    scheduler_schedule(t);
}

void irq_uart1() {
    uint* const irqr = (uint*)(UART1_BASE + UART_INTR_OFFSET);
    const uint irq = uart_get_interrupt(*irqr);
    const char modem_mesg[] = "UART1 Modem";

    switch (irq) {
    case 1:     // RECEIVE TIMEOUT
    case 3:     // RECEIVE
        irq_uart1_recv();
        break;
    case 2:     // TRANSMIT
        irq_uart1_send();
        break;
    case 4:     // MODEM
        *irqr = (uint)irqr;
        kprintf(sizeof(modem_mesg), modem_mesg);
        break;
    }
}

