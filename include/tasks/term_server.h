
#ifndef __TERM_SERVER_H__
#define __TERM_SERVER_H__

void __attribute__ ((noreturn)) term_server(void);

int get_term_char(void);
int put_term_char(char ch);
int Puts(char* const str, int length);

#endif

