
#include <rand.h>
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

    k2_signup(s_id);
    for(;;) {

        k2rps_move move = (k2rps_move) abs(rand()%3);

        switch( k2_play(s_id, move) ) {
        case K2_RPS_WIN:
            vt_log("RPS-C(%d):\tI ARE WINRAR!", id);
            break;
        case K2_RPS_LOSE:
            vt_log("RPS-C(%d):\tI ARE SAD!", id);
            break;
        case K2_RPS_TIE:
            vt_log("RPS-C(%d):\tgame tie...", id);
            break;
        case K2_RPS_QUIT:
            vt_log("RPS-C(%d):\tQuitting!", id);
            return;
        default:
            vt_log("RPS-C(%d):\tUnexpected!", id);
        }
        vt_flush();
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

    k2_signup(s_id);
    for (;;) {
        k2rps_move move = INVALID;
        while( move == INVALID ) {
            vt_log("RPS-H(%d):\t(R)ock (P)aper or (S)cissors?", id);
            vt_flush();

            char input = vt_waitget();
            if(input == 'q') {
                vt_log("RPS-H(%d):\tCalled Exit", id);
                vt_flush();
                k2_quit(s_id);
                return;
            }
            move = _ctom(input);
        }

        switch(k2_play(s_id, move)) {
        case K2_RPS_WIN:
            vt_log("RPS-H(%d):\tYOU'RE WINNER", id);
            break;
        case K2_RPS_TIE:
            vt_log("RPS-H(%d):\tTie, so everyone loses", id);
            break;
        case K2_RPS_LOSE:
            vt_log("RPS-H(%d):\tYOU ARE A FAILURE!", id);
            break;
        case K2_RPS_QUIT:
            vt_log("RPS-H(%d):\tGame Over", id);
            return;
        default:
            vt_log("RPS-H(%d):\tUNEXPECTED", id);
            break;
        }
        vt_flush();
    }
}

