#include <std.h>
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
    int       size;
    char      payload[4];
} train_req;

struct in {
    char_buffer b;
    char buffer[16];
};

struct out {
    char_buffer b;
    char buffer[128];
};

static void write_carrier() {
    int ptid      = myParentTid();
    volatile int* const uart_flag = (int*)(UART1_BASE + UART_FLAG_OFFSET);

    train_req buffer = {
        .type = CARRIER,
        .size = 0
    };

    FOREVER {
        buffer.size = Send(ptid,
                           (char*)&buffer, sizeof(buffer),
                           buffer.payload, sizeof(buffer.payload));        
        assert(buffer.size > 0, "Failed to send to train (%d)", buffer.size);

        if (!(*uart_flag & CTS_MASK)) {
            AwaitEvent(UART1_MODM, NULL, 0);
        }

        AwaitEvent(UART1_SEND, buffer.payload, buffer.size);

        if (*uart_flag & CTS_MASK) {
            AwaitEvent(UART1_MODM, NULL, 0);
        }
    }
}

static void receive_notifier() {
    int ptid = myParentTid();
    train_req buffer = {
        .type = NOTIFIER,
        .size = 0
    };

    FOREVER {
       buffer.size = AwaitEvent(UART1_RECV,
                                buffer.payload,
                                sizeof(buffer.payload));
       int result  = Send(ptid, (char*)&buffer, sizeof(buffer), NULL, 0);

       UNUSED(result);
       assert(!result, "failed to notify train server (%d)", result);
    }
}

void train_server() {
    int tid;
    train_req req;
    
    const int my_tid   = myTid();
    int send_tid = my_tid;
    int get_tid  = my_tid;

    tid = RegisterAs(TRAIN_SEND);
    assert(tid == 0, "Train Server failed to register send name (%d)", tid);

    tid = RegisterAs(TRAIN_RECV);
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
        case CARRIER:
            if (cbuf_can_consume(&train_out.b)) {
                uint c = (uint)cbuf_consume(&train_out.b);
                Reply(tid, (char*)&c, sizeof(uint));
            } else {
                send_tid = tid;
            }
            break;

        case NOTIFIER:
            Reply(tid, NULL, 0);

            for (int i = 0; i < req.size; i++) {
                cbuf_produce(&train_in.b, req.payload[i]);
            }

            if (get_tid != my_tid) {
                uint c = (uint)cbuf_consume(&train_in.b);
                Reply(get_tid, (char*)&c, sizeof(c));
                get_tid = my_tid; 
            }
            break;

        case GET:
            if (cbuf_can_consume(&train_in.b)) {
                uint c = (uint)cbuf_consume(&train_in.b);
                Reply(tid, (char*)&c, sizeof(c));
            } else {
                assert(get_tid == my_tid,
                       "task %d already waiting for train input", get_tid);
                get_tid = tid;
            }
            break;

        case PUT:
            Reply(tid, NULL, 0);
            for(int i = 0; i < req.size; i++) {
                cbuf_produce(&train_out.b, req.payload[i]);
            }
            
            if (send_tid != my_tid) {
                uint c = cbuf_consume(&train_out.b);
                Reply(send_tid, (char*)&c, sizeof(c));
                send_tid = my_tid;
            }
            break;
        }
    }
}

int put_train(int tid, char c){
    train_req req = {
        .type       = PUT,
        .size       = 1,
        .payload[0] = c
    };

    return Send(tid, (char*)&req, sizeof(req), NULL, 0);
}

int get_train(int tid, char *buf, size buf_size) {
    train_req req = {
        .type       = GET,
        .size       = 0
    };
    
    return Send(tid, (char*)&req, sizeof(req), buf, buf_size);
}

