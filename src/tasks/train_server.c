#include <std.h>
#include <train.h>
#include <debug.h>
#include <vt100.h>
#include <ts7200.h>
#include <syscall.h>
#include <scheduler.h>

#include <circular_buffer.h>
#include <tasks/train_server.h>

typedef enum {
    CARRIER,
    NOTIFIER,
    PUT,
    GET
} treq_type;

typedef struct {
    treq_type type;
    struct {
        int  size;
        char data[4];
    } payload;
} train_req;

struct in {
    char_buffer b;
    char buffer[16];
};

struct out {
    char_buffer b;
    char buffer[64];
};

static void __attribute__((noreturn)) write_carrier() {
    int ptid = myParentTid();
    train_req req = {
        .type    = CARRIER,
        .payload = {
            .size = 0,
            .data = {0}
        }
    };

    struct {
        int  size;
        char data[4];
    } buffer;

    char *next;
    FOREVER {
        int ret = Send(ptid,
                       (char*)&req, sizeof(req),
                       (char*)&buffer, sizeof(buffer));
        
        assert(ret == sizeof(buffer),
                "Failed to send to train (%d)", buffer.size);
        assert(buffer.size > 0 && buffer.size <= 4,
                "Train Carrier Invalid Send Size (%d)", buffer.size);

        next = buffer.data;
        for(int i = 0; i < buffer.size;) {
            AwaitEvent(UART1_CTS, NULL, 0);
            
            ret = AwaitEvent(UART1_SEND, next, buffer.size);

            assert(ret > 0 && ret <= 4, "Sending To Train Failed (%d)", ret);
            next += ret;
            i    += ret;

            AwaitEvent(UART1_DOWN, NULL, 0);
        }
    }
}

static void __attribute__((noreturn)) receive_notifier() {
    int ptid = myParentTid();
    train_req req = {
        .type = NOTIFIER,
        .payload = {
            .size = 4,
            .data = {0, 0, 0, 0}
        }
    };

    FOREVER {
        req.payload.size = AwaitEvent(UART1_RECV,
                                      req.payload.data,
                                      sizeof(req.payload.data));

        klog("received %d %d", req.payload.size, (char)*req.payload.data);
        int result = Send(ptid, (char*)&req, sizeof(req), NULL, 0);

        UNUSED(result);
        assert(!result, "failed to notify train server (%d)", result);
    }
}

void train_server() {
    int tid;
    train_req req;
    
    const int my_tid = myTid();
    klog("train server started at %d", my_tid);

    int send_tid     = -1;
    int get_tid      = -1;

    tid = RegisterAs((char*)TRAIN_SEND);
    assert(tid == 0, "Train Server failed to register send name (%d)", tid);

    tid = RegisterAs((char*)TRAIN_RECV);
    assert(tid == 0, "Train Server failed to register receive name (%d)", tid);
    
    tid = Create(TASK_PRIORITY_HIGH, write_carrier);
    assert(tid>0, "Failed to start up train write carrier (%d)", tid);

    tid = Create(TASK_PRIORITY_HIGH, receive_notifier);
    assert(tid>0, "Failed to start up train read notifier (%d)", tid);

    struct in  train_in;
    struct out train_out;

    cbuf_init(&train_in.b,  sizeof(train_in.buffer),  train_in.buffer);
    cbuf_init(&train_out.b, sizeof(train_out.buffer), train_out.buffer);

    FOREVER {
        Receive(&tid, (char*)&req, sizeof(req));

        switch (req.type) {
        case CARRIER: {
            assert(send_tid == -1, "Double Registered Send %d %d", tid, send_tid);

            int  i;
            for(i = 0; i<4 && cbuf_can_consume(&train_out.b); i++) {
                req.payload.data[i] = cbuf_consume(&train_out.b);
            }
            if(i > 0) {
                req.payload.size = i;
                Reply(tid, (char*)&req.payload, sizeof(req.payload));
            } else {
                send_tid = tid;
            }
            break;
        }
        case NOTIFIER:
            Reply(tid, NULL, 0);

            for (int i = 0; i < req.payload.size; i++) {
                cbuf_produce(&train_in.b, req.payload.data[i]);
            }

            if (get_tid != -1) {
                int i;
                char c[4] = {0};
                
                for(i = 0; i<4 && cbuf_can_consume(&train_out.b) ; i++) {
                    c[i] = cbuf_consume(&train_out.b);
                }
                Reply(get_tid, c, i);
                get_tid = -1;
            }
            break;

        case GET:
            if (cbuf_can_consume(&train_in.b)) {
                uint c = (uint)cbuf_consume(&train_in.b);
                Reply(tid, (char*)&c, sizeof(char));
            } else {
                assert(get_tid == -1,
                       "task %d already waiting for train input", get_tid);
                get_tid = tid;
            }
            break;

        case PUT:
            assert(req.payload.size > 0 && req.payload.size <= 4, 
                   "Invalid train req size (%d)", req.payload.size);
            Reply(tid, NULL, 0);
            for(int i = 0; i < req.payload.size; i++) {
                cbuf_produce(&train_out.b, req.payload.data[i]);
            }
            
            if (-1 != send_tid) {
                int  i;
                for(i = 0; i<4 && cbuf_can_consume(&train_out.b) ; i++) {
                    req.payload.data[i] = cbuf_consume(&train_out.b);
                }
                if(i > 0) {
                    req.payload.size = i;
                    Reply(send_tid, (char*)&req.payload, sizeof(req.payload));
                    send_tid = -1;
                }
            }
            break;
        }
    }
}

int put_train_char(int tid, char c) {
    train_req req = {
        .type    = PUT,
        .payload = {
            .size = 1,
            .data = {c, 0, 0, 0}
        }
    };

    return Send(tid, (char*)&req, sizeof(req), NULL, 0);
}

int put_train_cmd(int tid, char cmd, char vctm) {
    train_req req = {
        .type    = PUT,
        .payload = {
            .size = 2,
            .data = {cmd, vctm, 0, 0}
        }
    };

    return Send(tid, (char*)&req, sizeof(req), NULL, 0);
}

int put_train_turnout(int tid, char cmd, char turn) {
    train_req req = {
        .type = PUT,
        .payload = {
            .size = 3,
            .data = {cmd, turn, TURNOUT_CLEAR, 0}
        }
    };

    return Send(tid, (char*)&req, sizeof(req), NULL, 0);
}

int get_train(int tid, char *buf, int buf_size) {
    assert(buf_size > 0 && buf_size <= 10,
           "Tried to read invalid sensor bank size (%d)", buf_size);

    train_req req = {
        .type    = GET,
        .payload = {
            .size = buf_size
        }
    };

    return Send(tid, (char*)&req, sizeof(req), buf, buf_size);
}

