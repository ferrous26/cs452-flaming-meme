#include <std.h>
#include <train.h>
#include <debug.h>
#include <syscall.h>
#include <char_buffer.h>
#include <tasks/train_server.h>
#include <tasks/name_server.h>
#include <tasks/term_server.h>

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

CHAR_BUFFER(128)
static int train_server_tid;

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

    buffer.size = RegisterAs((char*)TRAIN_CARRIER_NAME);
    assert(buffer.size == 0,
           "Train Carrier Failed to Register (%d)", buffer.size);

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

    req.payload.size = RegisterAs((char*) TRAIN_NOTIFIER_NAME);
    assert(req.payload.size == 0,
           "Train Notifier Failed to Register (%d)", req.payload.size);

    FOREVER {
        req.payload.size = AwaitEvent(UART1_RECV,
                                      req.payload.data,
                                      sizeof(req.payload.data));

        int result = Send(ptid, (char*)&req, sizeof(req), NULL, 0);

        UNUSED(result);
        assert(!result, "failed to notify train server (%d)", result);
    }
}

inline static void _startup() {
    train_server_tid = myTid();

    int tid = RegisterAs((char*)TRAIN_SEND_NAME);
    assert(tid == 0, "Train Server failed to register send name (%d)", tid);

    tid = RegisterAs((char*)TRAIN_RECV_NAME);
    assert(tid == 0, "Train Server failed to register receive name (%d)", tid);

    tid = Create(TASK_PRIORITY_HIGH, write_carrier);
    assert(tid>0, "Failed to start up train write carrier (%d)", tid);

    tid = Create(TASK_PRIORITY_HIGH, receive_notifier);
    assert(tid>0, "Failed to start up train read notifier (%d)", tid);
}

struct train_context {
    int        get_tid;
    int        send_tid;

    char_buffer train_in;
    char_buffer train_out;
};

void train_server() {
    _startup();

    int tid;
    train_req req;
    struct train_context context = {
        .send_tid = -1,
        .get_tid  = -1
    };

    cbuf_init(&context.train_in);
    cbuf_init(&context.train_out);

    FOREVER {
        Receive(&tid, (char*)&req, sizeof(req));

        switch (req.type) {
        case CARRIER: {
            assert(-1 == context.send_tid,
                   "Double Registered Send %d %d",
                   tid, context.send_tid);

            int  i;
            for(i = 0; i<4 && cbuf_count(&context.train_out); i++) {
                req.payload.data[i] = cbuf_consume(&context.train_out);
            }
            if(i > 0) {
                req.payload.size = i;
                Reply(tid, (char*)&req.payload, sizeof(req.payload));
            } else {
                context.send_tid = tid;
            }
            break;
        }
        case NOTIFIER:
            Reply(tid, NULL, 0);

            assert(req.payload.size > 0 && req.payload.size <= 4,
                    "Invalid size sent from server");

            for (int i = 0; i < req.payload.size; i++)
                cbuf_produce(&context.train_in, req.payload.data[i]);

            if (context.get_tid != -1) {
                char c[4];
                c[0] = cbuf_consume(&context.train_in);
                Reply(context.get_tid, c, 1);
                context.get_tid = -1;
            }
            break;

        case GET:
            if (cbuf_count(&context.train_in)) {
                char c[4];
                c[0] = cbuf_consume(&context.train_in);
                Reply(tid, c, 1);
            } else {
                assert(context.get_tid == -1,
                       "task %d already waiting for train input",
                       context.get_tid);
                context.get_tid = tid;
            }
            break;

        case PUT:
            assert(req.payload.size > 0 && req.payload.size <= 4,
                   "Invalid train req size (%d)", req.payload.size);
            Reply(tid, NULL, 0);
            for(int i = 0; i < req.payload.size; i++)
                cbuf_produce(&context.train_out, req.payload.data[i]);

            if (-1 != context.send_tid) {
                int  i;
                for(i = 0; i<4 && cbuf_count(&context.train_out) ; i++) {
                    req.payload.data[i] = cbuf_consume(&context.train_out);
                }
                if(i > 0) {
                    req.payload.size = i;
                    Reply(context.send_tid,
                          (char*)&req.payload, sizeof(req.payload));
                    context.send_tid = -1;
                }
            }
            break;
        }
    }
}

int put_train_char(char c) {
    train_req req = {
        .type    = PUT,
        .payload = {
            .size = 1,
            .data = {c, 0, 0, 0}
        }
    };

    return Send(train_server_tid, (char*)&req, sizeof(req), NULL, 0);
}

int put_train_cmd(char vctm, char cmd) {
    train_req req = {
        .type    = PUT,
        .payload = {
            .size = 2,
            .data = {cmd, vctm, 0, 0}
        }
    };

    return Send(train_server_tid, (char*)&req, sizeof(req), NULL, 0);
}

int put_train_turnout(char turn, char cmd) {
    train_req req = {
        .type = PUT,
        .payload = {
            .size = 3,
            .data = {cmd, turn, TURNOUT_CLEAR, 0}
        }
    };

    return Send(train_server_tid, (char*)&req, sizeof(req), NULL, 0);
}

int get_train_char() {
    char c;
    train_req req = {
        .type    = GET,
        .payload = {
            .size = 1
        }
    };

    int result = Send(train_server_tid,
                      (char*)&req, sizeof(req),
                      &c, sizeof(c));
    if(result < 0) return result;
    return c;
}

int get_train(char *buf, int buf_size) {
    assert(buf_size > 0 && buf_size <= 10,
           "Tried to read invalid sensor bank size (%d)", buf_size);

    train_req req = {
        .type    = GET,
        .payload = {
            .size = buf_size
        }
    };

    return Send(train_server_tid, (char*)&req, sizeof(req), buf, buf_size);
}
