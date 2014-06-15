
#ifndef __TRAIN_SERVER_H__
#define __TRAIN_SERVER_H__

#define TRAIN_SEND "W_TRAIN"
#define TRAIN_RECV "R_TRAIN"

void train_server(void);

int put_train_char(int tid, char c);
int put_train_cmd(int tid, char cmd, char vctm);
int put_train_turnout(int tid, char cmd, char turn);

int get_train(int tid, char *c, size buf_size);

#endif

