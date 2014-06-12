
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <scheduler.h>
#include <circular_buffer.h>

typedef enum {
    SEND,
    RECV,
    CARRIER,
    NOTIFIER
} req_type;

typedef struct {
    req_type type;
    int      size;
    char     payload[8];
} term_req;

static void recv_notifier() {
    int ptid = myParentTid();

    term_req buffer = {
        .type = NOTIFIER,
        .size = 0
    };

    FOREVER {
        buffer.size = AwaitEvent(UART2_RECV, 
                                 buffer.payload,
                                 sizeof(buffer.payload));
        if(Send(ptid, (char*)&buffer, sizeof(buffer), NULL, 0) < 0) break;
    }
}

static void send_carrier() {
    int ptid = myParentTid();
    term_req buffer = {
        .type = CARRIER,
        .size = 0
    };

    FOREVER {
        buffer.size = Send(ptid, 
                           (char*)&buffer, sizeof(buffer),
                           buffer.payload, sizeof(buffer.payload));
        
        if (buffer.size < 0) {
            debug_log("Send Dying! %d", buffer.size);
            break;
        }
        AwaitEvent(UART2_SEND, buffer.payload, buffer.size);
    }
}

struct out {
    char_buffer o;
    char buffer[1028];
};

struct in {
    char_buffer i;
    char buffer[16];
};

void term_server() {
    int my_tid = myTid();
    int out_tid = my_tid;

    int tid = Create(TASK_PRIORITY_HIGH, recv_notifier);
    tid = Create(TASK_PRIORITY_HIGH, send_carrier);
    
    vt_log("TERM: Started!");

    term_req req;
    struct in vt_in;
    struct out vt_out;

    cbuf_init(&vt_in.i,  sizeof(vt_in.buffer),  vt_in.buffer);
    cbuf_init(&vt_out.o, sizeof(vt_out.buffer), vt_out.buffer);

    FOREVER {
        Receive(&tid, (char*)&req, sizeof(req));

        switch(req.type) {
        case NOTIFIER:
            cbuf_produce(&vt_out.o, req.payload[0]);

            if (out_tid != my_tid) {
                uint c = (uint)cbuf_consume(&vt_out.o);
                Reply(out_tid, (char*)&c, sizeof(uint));
                out_tid = my_tid;
            }
            Reply(tid, NULL, 0);
            break;

        case CARRIER:
            if(cbuf_can_consume(&vt_out.o)) {
                uint c = (uint)cbuf_consume(&vt_out.o);
                Reply(tid, (char*)&c, sizeof(uint));
            } else {
                out_tid = tid;
            }
            break;

        case SEND:
            debug_log("got send!");
            Reply(tid, NULL, 0);
            break;

        case RECV:
            debug_log("got recv!");
            Reply(tid, NULL, 0);
            break;        
        }
    }
}

