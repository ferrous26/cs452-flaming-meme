
#ifndef __TERM_SERVER_H__
#define __TERM_SERVER_H__

typedef enum {
    GETC     = 1,
    PUTC     = 2,
    PUTS     = 3,
    CARRIER  = 4,
    NOTIFIER = 5
} term_req_type;

typedef union {
    char text[8];
    char* string;
} term_payload;

typedef struct {
    term_req_type type;
    int           size;
    term_payload  payload;
} term_req;

extern int term_server_tid;

void __attribute__ ((noreturn)) term_server(void);

#endif
