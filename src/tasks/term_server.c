#include <tasks/term_server.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <scheduler.h>
#include <circular_buffer.h>


#define TERM_ASSERT(expr, var) if (expr) term_failure(var, __LINE__)
static void __attribute__ ((noreturn)) term_failure(int result, uint line) {
    UNUSED(result);
    UNUSED(line);

    kdebug_log("Action failed in terminal server at line %u (%d)", line, result);
    Shutdown();
}


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
	TERM_ASSERT(result, result);
    }
}

// data going to the UART
static void __attribute__ ((noreturn)) send_carrier() {
    int ptid = myParentTid();
    term_req buffer = {
        .type = CARRIER,
        .size = 1 // TODO: set this to UART FIFO size when FIFO is enabled
    };

    FOREVER {
        buffer.size = Send(ptid,
                           (char*)&buffer, sizeof(term_req_type) + sizeof(int),
                           buffer.payload.text, sizeof(buffer.payload));
	TERM_ASSERT(buffer.size <= 0, buffer.size);

        int result = AwaitEvent(UART2_SEND, buffer.payload.text, buffer.size);
	TERM_ASSERT(result != 1, result);
    }
}

static void term_server_startup() {
    log("Terminal Server started at %d", myParentTid());
}

#define IN_BUFFER_SIZE  72
#define OUT_BUFFER_SIZE 1024
struct term_state {
    char* out_head;
    char* out_tail;
    char* in_head;
    char* in_tail;
    int   in_q;
    int   out_q;
    char  in_buffer[IN_BUFFER_SIZE];
    char  out_buffer[OUT_BUFFER_SIZE];
};

int term_server_tid;


void term_server() {

    term_server_tid = myTid();

    struct term_state state;
    state.out_head = state.out_tail = state.out_buffer;
    state.in_head  = state.in_tail  = state.in_buffer;
    state.in_q     = -1;
    state.out_q    = -1;

    // shortcuts to first address past the end of the buffer
    char* out_end  = state.out_buffer + OUT_BUFFER_SIZE;
    char* in_end   = state.in_buffer  + IN_BUFFER_SIZE;

    term_req req;

    int tid = Create(TASK_PRIORITY_HIGH, recv_notifier);
    TERM_ASSERT(tid < 0, tid);

    tid = Create(TASK_PRIORITY_HIGH, send_carrier);
    TERM_ASSERT(tid < 0, tid);

    tid = Create(TASK_PRIORITY_EMERGENCY, term_server_startup);
    TERM_ASSERT(tid < 0, tid);


    FOREVER {
	int result = Receive(&tid, (char*)&req, sizeof(req));

	assert(req.type >= GETC && req.type <= NOTIFIER,
	       "Invalid message type sent to terminal server (%d)", req.type);

        switch(req.type) {
	case GETC: {
	    if (state.in_head != state.in_tail) {
		result = Reply(tid, state.in_tail++, 1);
		TERM_ASSERT(result, result);

		if (state.in_tail == in_end)
		    state.in_tail = state.in_buffer;
	    }
	    else { // need to queue the request
		assert(state.in_q == -1,
		       "Multiple tasks waiting for input: %d and %d",
		       state.in_q, tid);
		state.in_q = tid;
	    }
	    break;
	}
	case PUTC: {
	    // if it can get passed to the send carrier right away
	    if (state.out_q != -1) {
		// then send it there immediately
		char byte = (char)req.size;
		result = Reply(state.out_q, &byte, 1);
		TERM_ASSERT(result, result);
		state.out_q = -1; // and reset the wait state
	    }
	    // else, buffer it
	    else {
		*state.out_head++ = (char)req.size;
		if (state.out_head == out_end)
		    state.out_head = state.out_buffer;
	    }

	    result = Reply(tid, NULL, 0);
	    TERM_ASSERT(result, result);
	    break;
	}
	case PUTS: {
	    // if we do not have enough space at the end of the buffer
	    if ((state.out_head + req.size) >= out_end) {
		// then we need to split up the string into chunks
		int chunk1 = out_end  - state.out_head;
		int chunk2 = req.size - chunk1;

                TERM_ASSERT(chunk1 == 0, chunk1);
		memcpy(state.out_head, req.payload.string, chunk1);
		// avoid the zero-length memcpy issue...
		if (chunk2)
		    memcpy(state.out_buffer,
			   req.payload.string + chunk1,
			   chunk2);

		state.out_head = state.out_buffer + chunk2;
	    }
	    // else, we are in the much nicer case :)
	    else {
		memcpy(state.out_head, req.payload.string, req.size);
		state.out_head += req.size;
	    }

	    // if the send carrier is waiting, then we can wake it up
	    if (state.out_q != -1) {
		result = Reply(state.out_q, state.out_tail++, 1);
		TERM_ASSERT(result, result);
		state.out_q = -1;
	    }

	    result = Reply(tid, NULL, 0);
	    TERM_ASSERT(result, result);
	    break;
	}
	case CARRIER: {
	    // if we have stuff in the out buffer, then feed it through
	    if (state.out_head != state.out_tail) {
		result = Reply(tid, state.out_tail++, 1);
		TERM_ASSERT(result, result);
		if (state.out_tail == out_end)
		    state.out_tail = state.out_buffer;
	    }
	    // else, we need to block the notifier task until we have something
	    else {
		state.out_q = tid;
	    }
	    break;
	}
	case NOTIFIER: {
	    // if someone is waiting, then send the byte directly to them
	    if (state.in_q >= 0) {
		result = Reply(state.in_q, req.payload.text, 1);
		TERM_ASSERT(result, result);
		state.in_q = -1;
	    }
	    // else we want to buffer the byte
	    else {
		*state.in_head++ = req.payload.text[0];
		if (state.in_head == in_end)
		    state.in_head = state.in_buffer;
	    }

	    result = Reply(tid, NULL, 0);
	    TERM_ASSERT(result, result);
            break;
	}
	}
    }
}
