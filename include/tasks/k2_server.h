
#ifndef __K2_SERVER_H__
#define __K2_SERVER_H__

#define K2_RPS_LOOK "K2_RPS"

#define K2_RPS_WIN  'w'
#define K2_RPS_TIE  't'
#define K2_RPS_LOSE 'l'
#define K2_RPS_FAIL 'f'
#define K2_RPS_PLAY 'p'
#define K2_RPS_QUIT 'q'

typedef enum {
    ROCK,
    PAPER,
    SCISSORS,
    INVALID
} k2rps_move;

int  k2_signup(int server_tid);
void k2_quit(int server_tid);
int  k2_play(int server_tid, k2rps_move);

void k2_server(void);

#endif

