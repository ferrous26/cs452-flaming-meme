#include <tasks/term_server.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <scheduler.h>
#include <circular_buffer.h>

// data coming from the UART
static void __attribute__ ((noreturn)) recv_notifier() {

    int ptid = myParentTid();

    term_req buffer = {
        .type = NOTIFIER,
        .size = 0
    };

    FOREVER {
        buffer.size = AwaitEvent(UART2_RECV,
                                 buffer.payload.text,
                                 sizeof(buffer.payload));
        int result = Send(ptid, (char*)&buffer, sizeof(buffer), NULL, 0);

	UNUSED(result);
	assert(!result, "Failed to notify terminal server (%d)", result);
    }
}

// data going to the UART
static void __attribute__ ((noreturn)) send_carrier() {
    int ptid = myParentTid();
    term_req buffer = {
        .type = CARRIER,
        .size = 0
    };

    FOREVER {
        buffer.size = Send(ptid,
                           (char*)&buffer, sizeof(buffer),
                           buffer.payload.text, sizeof(buffer.payload));

	assert(buffer.size > 0,
	       "Terminal server gave nothing to send carrier (%d)",
	       buffer.size);

        int result = AwaitEvent(UART2_SEND, buffer.payload.text, buffer.size);

	UNUSED(result);
	assert(!result,
	       "Failed to pass terminal output to the kernel (%d)", result);
    }
}

static void term_server_startup() {
    log("Terminal Server started at %d", myParentTid());
}

struct out {
    char_buffer o;
    char buffer[2048];
};

struct in {
    char_buffer i;
    char buffer[16];
};

int term_server_tid;

void term_server() {

    term_server_tid = myTid();

    int tid = Create(TASK_PRIORITY_HIGH, recv_notifier);
    assert(tid > 0, "Failed to startup the terminal receiver (%d)", tid);

    tid = Create(TASK_PRIORITY_HIGH, send_carrier);
    assert(tid > 0, "Failed to startup the terminal carrier (%d)", tid);

    tid = Create(TASK_PRIORITY_EMERGENCY, term_server_startup);
    assert(tid > 0, "Failed to run the terminal startup message (%d)", tid);

    term_req req;
    struct in vt_in;
    struct out vt_out;

    int waiter = -1;

    cbuf_init(&vt_in.i,  sizeof(vt_in.buffer),  vt_in.buffer);
    cbuf_init(&vt_out.o, sizeof(vt_out.buffer), vt_out.buffer);


    FOREVER {
	Receive(&tid, (char*)&req, sizeof(req));

	assert(req.type >= GETC && req.type <= NOTIFIER,
	       "Invalid message type sent to terminal server (%d)", req.type);

        switch(req.type) {
	case GETC: {
	    if (cbuf_can_consume(&vt_in.i)) {
		char byte = cbuf_consume(&vt_in.i);
		Reply(tid, &byte, sizeof(byte));
		// TODO: check error code
	    }
	    else {
		waiter = tid;
	    }
	    break;
	}
	case PUTC: {
	    char ch = (char)req.size;
	    uart2_bw_write(&ch, 1);
	    Reply(tid, NULL, 0);
	    // TODO: check result code
	    break;
	}

	case PUTS:
	    uart2_bw_write(req.payload.string, req.size);
	    Reply(tid, NULL, 0);
	    // TODO: check result code
	    break;

	case CARRIER:
	    // just block it for now
            /* if (cbuf_can_consume(&vt_out.o)) { */
            /*     uint c = (uint)cbuf_consume(&vt_out.o); */
            /*     Reply(tid, (char*)&c, sizeof(uint)); */
            /* } else { */
            /*     out_tid = tid; */
            /* } */
            break;

	case NOTIFIER: {
	    cbuf_produce(&vt_in.i, req.payload.text[0]);
            Reply(tid, NULL, 0);
	    // TODO: check error code

	    if (waiter >= 0) {
		char byte = cbuf_consume(&vt_in.i);
		kdebug_log("sending %c from %p", byte, &byte);
		Reply(waiter, &byte, sizeof(byte));
		// TODO: check error code
		waiter = -1;
	    }
            break;
	}
	}
    }
}
