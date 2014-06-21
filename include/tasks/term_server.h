
#ifndef __TERM_SERVER_H__
#define __TERM_SERVER_H__

#include <serial.h>
#include <vt100.h>

#define TERM_RECV_NAME     "S_TERM"
#define TERM_SEND_NAME     "S_TERM"
#define TERM_CARRIER_NAME  "TERM_C"
#define TERM_NOTIFIER_NAME "TERM_N"

#define Putc_TERMINAL(ch) put_term_char(ch)
#define Putc_2(ch)        Putc_TERMINAL(ch)
#define Getc_TERMINAL get_term_char()
#define Getc_2        Getc_TERMINAL

void __attribute__ ((noreturn)) term_server(void);

int get_term_char(void);
int put_term_char(char ch);
int Puts(char* const str, int length);

#endif
