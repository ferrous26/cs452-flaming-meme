
#include <vt100.h>
#include <syscall.h>

#include <tasks/k2_server.h>
#include <tasks/k2_client.h>

void k2_computer() {
    int id   = myTid();
    int s_id = WhoIs(K2_RPS_LOOK);

    if(s_id < 0) {
        vt_log("RPS Computer Cant Get RPS SERVER!!!", id);
        vt_flush();
        return;
    }

    for(;;) {
        k2_signup(s_id);

        vt_log("RPS Computer %d has played to %d", id, s_id);
        vt_flush();

        if( k2_play(s_id, ROCK) == 'q' ) return;
    }
}

static k2rps_move _ctom(char c) {
    switch(c){
    case 'r':
    case 'R':
        return ROCK;
    case 'p':
    case 'P':
        return PAPER;
    case 's':
    case 'S':
        return SCISSORS;
    default:
        return INVALID;
    }
}

void k2_human() {
    int id   = myTid();
    int s_id = WhoIs(K2_RPS_LOOK);

    for (;;) {
        k2_signup(s_id);
        k2rps_move move = INVALID;

        while( move == INVALID ) {
            vt_log("RPS Human %d (R)ock (P)aper or (S)cissors?", id);
            vt_flush();

            char input = vt_waitget();
            if(input == 'q') {
                vt_log("RPS Human %d Called Exit", id);
                vt_flush();
                k2_quit(s_id);
                return;
            }
            move = _ctom(input);
        }

        switch(k2_play(s_id, move)) {
        case K2_RPS_WIN:
            vt_log("RPS Human %d YOU'RE WINNER", id);
            vt_flush();
            break;
        case K2_RPS_TIE:
            vt_log("RPS Human %d Tie, so everyone loses", id);
            vt_flush();
            break;
        case K2_RPS_LOSE:
            vt_log("RPS Human %d loss", id);
            vt_flush();
            break;
        case K2_RPS_QUIT:
            vt_log("RPS Human %d opponent closed match", id);
            vt_flush();
            return;
        default:
            vt_log("RPS Human %d UNEXPECTED", id);
            return;
        }
    }
}

