
#ifndef __TRAIN_SERVER_H__
#define __TRAIN_SERVER_H__

#define TRAIN_SEND "W_TRAIN"
#define TRAIN_RECV "R_TRAIN"

void train_server(void);
int put_train(int tid, char c);
int get_train(int tid, char *c, size buf_size);

#endif

