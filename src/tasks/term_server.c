#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <std.h>
#include <debug.h>
#include <syscall.h>
#include <char_buffer.h>
#include <ts7200.h>

typedef enum {
    GETC     = 1,
    PUTS     = 2,
    CARRIER  = 3,
    NOTIFIER = 4,
    OBUFFER  = 5,
    IBUFFER  = 6
} term_req_type;

typedef struct {
    term_req_type type;
    uint          length;
    char*         string;
} term_req;

// forward declarations
struct term_state;
struct term_puts;

#define INPUT_BUFFER_SIZE  512
#define OUTPUT_BUFFER_SIZE 128
#define OUTPUT_Q_SIZE      TASK_MAX // at least 8 tasks will never Puts

BULK_CHAR_BUFFER(INPUT_BUFFER_SIZE)

// Information stored when a Puts request must block
typedef struct {
    int   tid;
    uint  length;
    char* string;
} term_puts;

// Circular buffer for blocked Puts requests
typedef struct {
    term_puts* head;
    term_puts* tail;
    term_puts* end;
    uint       count;
    term_puts  buffer[OUTPUT_Q_SIZE];
} puts_buffer;

// State for the terminal server
struct term_state {
    char* out_head;   // pointer where to insert data for output
    char* obuffer;
    char  obuffers[2][OUTPUT_BUFFER_SIZE];

    puts_buffer output_q;

    char_buffer input_q;
    char* recv_buffer;

    int   in_tid;     // tid for the Getc caller that was blocked
    int   carrier;    // tid for the carrier when it gets blocked
    int   notifier;   // tid for the notifier when it makes a request
};

/* Global data */
static int term_server_tid;

/* Circular buffer implementations */
static inline void pbuf_init(puts_buffer* const cb) {
    cb->head  = cb->tail = cb->buffer;
    cb->end   = cb->buffer + OUTPUT_Q_SIZE; // first address past the end
    cb->count = 0;
    memset(cb->buffer, 0, sizeof(cb->buffer));
}

static inline __attribute__ ((const))
uint pbuf_count(const puts_buffer* const cb) {
    return cb->count;
}

static inline void pbuf_produce(puts_buffer* const cb,
				const term_req* const req,
				const int tid) {

    term_puts* const new_puts = cb->head++;
    new_puts->tid    = tid;
    new_puts->length = req->length;
    new_puts->string = req->string;

    cb->count++;
    if (cb->head == cb->end) {
	cb->head = cb->buffer;
    }
    assert(cb->count < OUTPUT_Q_SIZE, "Overfilled buffer!")
}

static inline term_puts* pbuf_peek(puts_buffer* const cb) {
    return cb->tail;
}

static inline void pbuf_consume(puts_buffer* const cb) {
    assert(cb->count, "Trying to consume from empty puts queue");
    cb->tail++;
    cb->count--;
    if (cb->tail == cb->end)
	cb->tail = cb->buffer;
}


/* Static methods for the terminal server */


// There are some checks that we want to make that should always be used
// so we have a special assertion function that doesn't disappear during
// a release build
#define TERM_ASSERT(expr, var) if (!(expr))  term_failure(var, __LINE__)
static void __attribute__ ((noreturn)) term_failure(int result, uint line) {
    Abort(__FILE__, line, "Action failed in terminal server (%d)", result);
}

// data coming from the UART
static void __attribute__ ((noreturn)) recv_notifier() {
    int ptid = myParentTid(); // the term server

    // buffer of input
    char input[UART_FIFO_SIZE];

    term_req msg = {
        .type   = IBUFFER,
        .length = UART_FIFO_SIZE,
	.string = input
    };

    int result = RegisterAs((char*)TERM_NOTIFIER_NAME);
    assert(result == 0, "Terminal Notifier Failed to Register (%d)", result);
    // first, let the server know about the input buffer so that we don't need
    // to keep sending the pointer each time we notify the server of input
    result = Send(ptid,
		  (char*)&msg,  sizeof(msg),
		  (char*)input, sizeof(input));
    TERM_ASSERT(result == 0, result);

    // now, we can keep filling the buffer and sending over only the size
    // with the notification
    msg.type = NOTIFIER;

    FOREVER {
        msg.length = (uint)AwaitEvent(UART2_RECV, (char*)input, sizeof(input));
        result     = Send(ptid,
			  (char*)&msg, sizeof(term_req_type) + sizeof(int),
			  NULL, 0);
	TERM_ASSERT(result == 0, result); // we should never get anything back
    }
}

// handle the initial message from the receive notifier
static inline void _term_ibuffer(struct term_state* const state,
				 term_req* const req) {

    state->recv_buffer = req->string;

    int result = Reply(state->notifier, NULL, 0);
    TERM_ASSERT(result == 0, result);
}

// data going to the UART
static void __attribute__ ((noreturn)) send_carrier() {
    int ptid = myParentTid(); // the term server

    struct {
	char* one;
	char* two;
    } buffers;

    term_req msg = {
        .type = OBUFFER
    };

    int result = RegisterAs((char*)TERM_CARRIER_NAME);
    assert(result == 0, "Terminal Carrier Failed to Register (%d)", result);
    // first, ask the server for the buffer locations so that we can avoid
    // having to send the buffer pointers over and over again
    result = Send(ptid,
	          (char*)&msg,     sizeof(term_req_type),
		  (char*)&buffers, sizeof(buffers));
    TERM_ASSERT(result == sizeof(buffers), result);

    // now, we can just keep swapping between buffers every time that we
    // send data, thus avoiding one copy of the buffer and the need to
    // send the buffer pointers, all we need is the size
    msg.type = CARRIER;
    char* buffer = buffers.two;

    FOREVER {
	// blocked until a buffer is ready to go
        result = Send(ptid,
		      (char*)&msg,        sizeof(term_req_type),
		      (char*)&msg.length, sizeof(int));
	TERM_ASSERT(result == sizeof(int), result);

	assert(msg.length > 0 && msg.length <= OUTPUT_BUFFER_SIZE,
	       "BAD CARRIER SIZE (%d)", msg.length);

	// switch to next buffer
	if (buffer == buffers.two)
	    buffer = buffers.one;
	else
	    buffer = buffers.two;

	// now, pump the buffer through to the kernel
	uint chunk = 0;
	while (msg.length) {
	    result = AwaitEvent(UART2_SEND, buffer + chunk, (int)msg.length);
	    TERM_ASSERT(result > 0, result);

	    chunk      += (uint)result;
	    msg.length -= (uint)result;
	}
    }
}

// handle the init message from the server
static inline void _term_obuffer(struct term_state* const state) {

    // need to send back the pointers to the obuffers
    struct {
	char* one;
	char* two;
    } buffers = {
	.one = state->obuffers[0],
	.two = state->obuffers[1]
    };

    int result = Reply(state->carrier, (char*)&buffers, sizeof(buffers));
    TERM_ASSERT(result == 0, result);

    state->carrier = -1;
}

static inline void _term_try_send(struct term_state* const state) {

    // at this point the carrier is ready, so we want to copy
    // as much crap into the buffer as we can, and then reply to
    // the carrier

    // how much space we have in the buffer
    uint space = OUTPUT_BUFFER_SIZE - (uint)(state->out_head - state->obuffer);

    // if we have nothing to send to the carrier
    if (space == OUTPUT_BUFFER_SIZE && pbuf_count(&state->output_q) == 0)
	return; // then we need to block the carrier

    // keep going until we run out of space and still have peeps to consume
    while (space && pbuf_count(&state->output_q)) {

	term_puts* const puts = pbuf_peek(&state->output_q);
	uint chunk = space;

	// if we have more space than needed
	if (chunk > puts->length)
	    chunk = puts->length; // then only take what is needed

	// copy what we can into the buffer
	memcpy(state->out_head, puts->string, chunk);

	// move these up so we don't overwrite things...
	state->out_head += chunk;
	puts->length    -= chunk;
	puts->string    += chunk;
	space           -= chunk;

	// if we have consumed the entire string
	if (puts->length == 0) {
	    pbuf_consume(&state->output_q);

	    int result = Reply(puts->tid, NULL, 0);
	    TERM_ASSERT(result == 0, result);
	}
    }

    // then do carrier-send dance
    space = OUTPUT_BUFFER_SIZE - space;
    int result = Reply(state->carrier, (char*)&space, sizeof(uint));
    TERM_ASSERT(result == 0, result);

    // then do the buffer-swap dance
    if (state->obuffer == state->obuffers[0])
	state->obuffer  = state->obuffers[1];
    else
	state->obuffer  = state->obuffers[0];
    state->out_head     = state->obuffer;

    // reset this now (before calling _term_try_puts)
    state->carrier = -1;
}

static inline void _term_try_puts(struct term_state* const state,
				  const term_req* const req,
				  const int tid) {
    pbuf_produce(&state->output_q, req, tid);

    // if the carrier happened to be waiting
    if (state->carrier >= 0)
	// then we can wake him up and send some stuff
	_term_try_send(state);
}

static inline void _term_try_getc(struct term_state* const state) {

    // if we have a buffered byte
    if (cbuf_count(&state->input_q)) {
	// then send it over the wire immediately
	char  byte = cbuf_consume(&state->input_q);
	int result = Reply(state->in_tid, &byte, 1);
	TERM_ASSERT(result == 0, result);

	// don't forget to reset this
	state->in_tid = -1;

	// NOTE: if we blocked the receiver for filling the buffer,
	// this is where we would want to unblock the receiver if
	// the buffer has been sufficiently emptied
    }
}

static inline void _term_recv(struct term_state* const state,
			      const uint length) {

    if (length > 1)
	cbuf_bulk_produce(&state->input_q, state->recv_buffer, length);
    else
	cbuf_produce(&state->input_q, *state->recv_buffer);

    // reply right away in case there is already more crap waiting
    int result = Reply(state->notifier, NULL, 0);
    TERM_ASSERT(result == 0, result);

    // if we are past the fill threshold, then we need to send
    // the XOFF byte in the kernel handler
    assert(cbuf_count(&state->input_q) < INPUT_BUFFER_SIZE,
	   "Filled the input buffer");

    // if someone was waiting for a byte
    if (state->in_tid >= 0)
	_term_try_getc(state);
}

static inline void _startup() {
    term_server_tid = myTid();

    int tid = RegisterAs((char*)TERM_SEND_NAME);
    assert(tid == 0, "Terminal Failed to register send name (%d)", tid);

    tid = RegisterAs((char*)TERM_RECV_NAME);
    assert(tid == 0, "Terminal Failed to register receive name (%d)", tid);

    tid = Create(TASK_PRIORITY_HIGH, recv_notifier);
    TERM_ASSERT(tid > 0, tid);

    tid = Create(TASK_PRIORITY_HIGH, send_carrier);
    TERM_ASSERT(tid > 0, tid);
}


void term_server() {
    _startup();

    struct term_state state;

    // initialize this mofo
    memset(&state, 0, sizeof(state));
    state.out_head = state.obuffer = state.obuffers[0];
    pbuf_init(&state.output_q);
    cbuf_init(&state.input_q);
    state.in_tid = state.carrier = state.notifier = -1;

    int tid = 0;
    term_req req;

    FOREVER {
#ifdef DEBUG
	memset(&req, 0, sizeof(req));
#endif
	int result = Receive(&tid, (char*)&req, sizeof(req));

	UNUSED(result);
	assert(req.type >= GETC && req.type <= IBUFFER,
	       "Invalid message type sent to terminal server (%d)", req.type);

        switch(req.type) {
	case GETC:
	    assert(state.in_tid == -1,
		   "Multiple tasks waiting for input: %d and %d",
		   state.in_tid, tid);
	    state.in_tid = tid;
	    _term_try_getc(&state);
	    break;

	case PUTS: {
	    _term_try_puts(&state, &req, tid);
	    break;
	}

	case CARRIER:
	    state.carrier = tid;
	    _term_try_send(&state);
	    break;

	case NOTIFIER:
	    _term_recv(&state, req.length);
	    break;

	case OBUFFER:
	    state.carrier = tid;
	    _term_obuffer(&state);
	    break;

	case IBUFFER:
	    state.notifier = tid;
	    _term_ibuffer(&state, &req);
	    break;
	}
    }
}

int put_term_char(char ch) {
    term_req req = {
	.type   = PUTS,
	.length = 1,
	.string = &ch
    };

    int result = Send(term_server_tid,
                      (char*)&req, sizeof(term_req),
		      NULL, 0);

    if (result == 0) return OK;

    assert(result != INCOMPLETE && result != INVALID_TASK,
	   "Terminal server died");

    return result;
}

int Puts(char* const str, int length) {

    assert(length > 0 && length < 4096,
	   "Bizarre string length sent to Puts (%d)", length);

    term_req req = {
	.type   = PUTS,
	.length = (uint)length,
	.string = str
    };

    int result = Send(term_server_tid,
		      (char*)&req, sizeof(term_req),
		      NULL, 0);

    if (result == 0) return OK;

    assert(result != INCOMPLETE && result != INVALID_TASK,
	   "Terminal server died");

    return result;
}

int get_term_char() {
    term_req req = {
	.type = GETC
    };

    char byte;
    int result = Send(term_server_tid,
                      (char*)&req, sizeof(term_req_type),
                      &byte, sizeof(byte));

    if (result > 0) return byte;

    assert(result != INCOMPLETE && result != INVALID_TASK,
	   "Terminal server died");

    return result;
}
