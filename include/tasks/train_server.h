
#ifndef __TRAIN_SERVER_H__
#define __TRAIN_SERVER_H__

#include <serial.h>

#define TRAIN_CARRIER_NAME  "TRAIN_C"
#define TRAIN_NOTIFIER_NAME "TRAIN_N"
#define TRAIN_SEND_NAME     "S_TRAIN"
#define TRAIN_RECV_NAME     "S_TRAIN"

#define Putc_TRAIN(ch)    put_train_char(ch)
#define Putc_1(ch)        Putc_train(ch);
#define Getc_TRAIN        get_train_char()
#define Getc_1            Getc_TRAIN

void __attribute__ ((noreturn)) train_server(void);

int put_train_char(char c);
int put_train_cmd(char cmd, char vctm);
int put_train_speed(char cmd, char vctm);
int put_train_turnout(char cmd, char turn);

int get_train(char* c, size_t buf_size);
int get_train_char(void);
int get_train_bank(void);

#endif
