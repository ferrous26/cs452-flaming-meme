
#ifndef __TRAIN_SERVER_H__
#define __TRAIN_SERVER_H__

#define TRAIN_SEND "W_TRAIN"
#define TRAIN_RECV "R_TRAIN"

void __attribute__ ((noreturn)) train_server(void);

int put_train_char(char c);
int put_train_cmd(char cmd, char vctm);
int put_train_turnout(char cmd, char turn);

int get_train_char();
int get_train(char *c, int buf_size);

#endif

