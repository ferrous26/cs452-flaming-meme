#include <std.h>
#include <syscall.h>

#include <io.h>
#include <vt100.h>
#include <debug.h>

#include <tasks/k2_server.h>

typedef enum {
    SIGNUP,
    PLAY,
    QUIT
} k2rps_command;

typedef struct {
    k2rps_command command;
    k2rps_move    move;
} k2rps_req;

typedef struct {
    int q[64];
    uint head;
    uint tail;
    size siz;
} k2_queue;

typedef struct {
    int p1;
    k2rps_move m1;
    int p2;
    k2rps_move m2;
} k2_game;

static void  _queue_insert(k2_queue* q, int task);
static int   _queue_consume(k2_queue* q);
static bool  _can_play(k2_queue* q, k2_game* g);
static void  _start_game(k2_queue* q, k2_game* g);
static char* _mtoa(k2rps_move move);
static void  _reset_game(k2_game* g);
static void  _play(k2_game* g);

static void _print(char* s) {
    vt_log("K2_RPS:\t%s", s);
    vt_flush();
}

int k2_signup(int server_tid) {
    k2rps_req req;
    req.command = SIGNUP;

    int ret;
    int res = Send(server_tid,
                   (char*)&req, sizeof(req),
                   (char*)&ret, sizeof(ret));

    if( res < 0 ) return res;
    if( ret == 'p' ) return 0;
    return -42;
}

void k2_quit(int server_tid) {
    k2rps_req req;
    req.command = QUIT;
    
    int trash;
    Send(server_tid, (char*)&req, sizeof(req), (char*)&trash, sizeof(trash));
}

int k2_play(int server_tid, k2rps_move move) {
    k2rps_req req;
    req.command = PLAY;
    req.move    = move;

    int ret;
    int res = Send(server_tid,
                   (char*)&req, sizeof(req),
                   (char*)&ret, sizeof(ret));

    if (res < 0) return res;
    return ret;
}

void k2_server() {
    RegisterAs(K2_RPS_LOOK);
    _print("Started!");

    int tid;
    k2rps_req buffer;

    k2_queue queue;
    queue.head = 0;
    queue.tail = 0;
    queue.siz  = 0;

    k2_game  context;
    _reset_game(&context);

    for(;;) {
        Receive(&tid, (char*)&buffer, sizeof(buffer));

        switch(buffer.command) {
        case SIGNUP:
            _queue_insert(&queue, tid);
            _start_game(&queue, &context);
            break;

        case PLAY:
            if (context.p1 == tid) {
                context.m1 = buffer.move;
                if(context.m2 != INVALID) {
                    _play(&context);
                    _start_game(&queue, &context);
                }
            } else if (context.p2 == tid) {
                context.m2 = buffer.move;
                if(context.m1 != INVALID) {
                    _play(&context);
                    _start_game(&queue, &context);
                }
            } else {
                const int reply = K2_RPS_FAIL;
                Reply(tid, (char*)&reply, sizeof(reply));
            }
            break;

        case QUIT: {
            const int reply = K2_RPS_QUIT;
            if( context.p1 == tid || context.p2 == tid ) {
                Reply(context.p1, (char*)&reply, sizeof(reply));
                Reply(context.p2, (char*)&reply, sizeof(reply));

                _reset_game(&context);
                _start_game(&queue, &context);
            } else {
                Reply(tid, (char*)&reply, sizeof(reply));
            }
            break;
        }
        default:
            break;
        }
    }
}

static void _queue_insert(k2_queue *q, int task) {
    q->q[q->tail] = task;
    q->tail = (q->tail+1) & 63;
    q->siz ++;
}

static int _queue_consume(k2_queue *q) {
    if( q->siz == 0 ) {
        _print("TRYING TO CONSUME EMPTY QUEUE!!!!");
        return -1;
    }

    q->siz--;
    int result = q->q[q->head];
    q->head = (q->head+1) & 63;
    return result;
}

static bool _can_play(k2_queue *q, k2_game *g) {
    return q->siz > 1 && g->p1 == g->p2;
}

static void _play(k2_game* g) {
    const int lose = K2_RPS_LOSE;
    const int win  = K2_RPS_WIN;
    const int tie  = K2_RPS_TIE;

    if (g->m1 == g->m2) {
        Reply(g->p1, (char*)&tie, sizeof(int));
        Reply(g->p2, (char*)&tie, sizeof(int));
    } else if (g->m1 == (g->m2+1)%3) {
        Reply(g->p1, (char*)&win,  sizeof(int));
        Reply(g->p2, (char*)&lose, sizeof(int));
    } else {
        Reply(g->p1, (char*)&lose, sizeof(int));
        Reply(g->p2, (char*)&win,  sizeof(int));
    }

    vt_log("");
    vt_log("");
    vt_log("");
    vt_log("");
    _print("ROCK PAPER SCISSORS!");
    vt_log("K2_RPS:\t%d: %s", g->p1, _mtoa(g->m1));
    vt_log("K2_RPS:\t%d: %s", g->p2, _mtoa(g->m2));
    vt_log("Press Any Key To Continue....");
    vt_flush();
    vt_waitget();

    g->m1 = INVALID;
    g->m2 = INVALID;
}

static void _start_game(k2_queue* q, k2_game* g) {
    if( ! _can_play(q, g) ) { return; }

    g->p1 = _queue_consume(q);
    g->p2 = _queue_consume(q);
    g->m1 = INVALID;
    g->m2 = INVALID;

    const int reply = K2_RPS_QUIT;
    Reply(g->p1, (char*)&reply, sizeof(int));
    Reply(g->p2, (char*)&reply, sizeof(int));
}

static void _reset_game(k2_game* g) {
    g->p1 = 0;
    g->p2 = 0;

    g->m1 = INVALID;
    g->m2 = INVALID;
}

static char* _mtoa(k2rps_move move) {
    switch(move) {
    case ROCK:
        return "Rock";
    case PAPER:
	return "Paper";
    case SCISSORS:
	return "Scissors";
    case INVALID:
    default:
        debug_log("K2_RPS:\t%d is invalid", move);

        return "";
    }
}

