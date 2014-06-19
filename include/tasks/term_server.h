
#ifndef __TERM_SERVER_H__
#define __TERM_SERVER_H__

#define TERM_RECV_NAME     "TERM_R"
#define TERM_SEND_NAME     "TERM_W"
#define TERM_CARRIER_NAME  "TERM_C"
#define TERM_NOTIFIER_NAME "TERM_N"

void __attribute__ ((noreturn)) term_server(void);

int get_term_char(void);
int put_term_char(char ch);
int Puts(char* const str, int length);

#endif

