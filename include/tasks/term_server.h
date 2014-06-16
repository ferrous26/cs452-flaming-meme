
#ifndef __TERM_SERVER_H__
#define __TERM_SERVER_H__

typedef enum {
    GETC     = 1,
    PUTS     = 2,
    CARRIER  = 3,
    NOTIFIER = 4,
    OBUFFER  = 5,
    IBUFFER  = 6
} term_req_type;

typedef struct {
    term_req_type type;
    int           length;
    char*         string;
} term_req;

extern int term_server_tid;

void __attribute__ ((noreturn)) term_server(void);

#endif
